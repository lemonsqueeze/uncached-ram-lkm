/**
 * @file    uncached_ram.c
 * @author  lemonsqueeze
 * @date    26 Jul 2017
 * @version 0.1
 * @brief Map uncached memory in userspace
 * @see http://github.com/lemonsqueeze/uncached_ram_lkm
*/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/page-flags.h>
#include <asm/cacheflush.h>

#include <linux/device.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/slab.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("lemonsqueeze");
MODULE_DESCRIPTION("Map uncached mem to userspace.");
MODULE_VERSION("0.1");

struct buffer {
        char **pages;
        int page_count;
};

struct client {
	struct buffer buffer;
	unsigned long vm_start;
};

/* From kernel Documentation/x86/pat.txt:
 *
 * Advanced APIs for drivers
 * -------------------------
 * A. Exporting pages to users with remap_pfn_range, io_remap_pfn_range,
 * vm_insert_pfn
 * 
 * Drivers wanting to export some pages to userspace do it by using mmap
 * interface and a combination of
 * 1) pgprot_noncached()
 * 2) io_remap_pfn_range() or remap_pfn_range() or vm_insert_pfn()
 * 
 * With PAT support, a new API pgprot_writecombine is being added. So, drivers can
 * continue to use the above sequence, with either pgprot_noncached() or
 * pgprot_writecombine() in step 1, followed by step 2.
 * 
 * In addition, step 2 internally tracks the region as UC or WC in memtype
 * list in order to ensure no conflicting mapping.
 * 
 * Note that this set of APIs only works with IO (non RAM) regions. If driver
 * wants to export a RAM region, it has to do set_memory_uc() or set_memory_wc()
 * as step 0 above and also track the usage of those pages and use set_memory_wb()
 * before the page is freed to free pool.
 */

/* insert_page() code is based on drivers/firewire/core-cdev.c */

static void buffer_destroy(struct buffer *buffer)
{
	int i;
	printk(KERN_INFO "Freeing pages\n");
	
	for (i = 0; i < buffer->page_count; i++)
		if (buffer->pages[i]) {
			char *addr = buffer->pages[i];

			set_memory_wb((unsigned long)addr, 1);
			ClearPageReserved(virt_to_page(addr));
			free_page((unsigned long)addr);
		}

	kfree(buffer->pages);
	buffer->pages = NULL;
	buffer->page_count = 0;
}


static int buffer_alloc(struct buffer *buffer, int page_count)
{
	int i;

	printk(KERN_INFO "Allocating %i pages\n", page_count);
	buffer->page_count = page_count;
	buffer->pages = kzalloc(page_count * sizeof(buffer->pages[0]), GFP_KERNEL);
	if (buffer->pages == NULL)
		return -ENOMEM;

	for (i = 0; i < page_count; i++) {
		char *addr = (char*) __get_free_page(GFP_KERNEL);
		if (addr == NULL)
			break;
		
		buffer->pages[i] = addr;
		SetPageReserved(virt_to_page(addr));
		if (set_memory_uc((unsigned long)addr, 1))
			break;
	}
	
	if (i < page_count) {
		buffer_destroy(buffer);
		return -ENOMEM;
	}

	return 0;
}


static int buffer_map_vma(struct buffer *buffer,
			  struct vm_area_struct *vma)
{
	unsigned long uaddr;
	int i, err;

	uaddr = vma->vm_start;
	for (i = 0; i < buffer->page_count; i++) {
		err = vm_insert_page(vma, uaddr, virt_to_page(buffer->pages[i]));
		if (err)
			return err;

		uaddr += PAGE_SIZE;
	}

	return 0;
}


int cdev_major;

static int device_op_open(struct inode *inode, struct file *file)
{
	struct client *client;

	client = kzalloc(sizeof(*client), GFP_KERNEL);
	if (client == NULL)
		return -ENOMEM;

	file->private_data = client;

	return nonseekable_open(inode, file);
}

static int device_op_release(struct inode *inode, struct file *file)
{
	struct client *client = file->private_data;

	if (client->buffer.pages)
		buffer_destroy(&client->buffer);

	kfree(client);	
	return 0;
}


static int device_op_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct client *client = file->private_data;
	unsigned long size;
	int page_count, ret;

	if (!(vma->vm_flags & VM_SHARED))
		return -EINVAL;

	if (vma->vm_start & ~PAGE_MASK)
		return -EINVAL;

	client->vm_start = vma->vm_start;
	size = vma->vm_end - vma->vm_start;
	page_count = size >> PAGE_SHIFT;
	if (size & ~PAGE_MASK)
		return -EINVAL;

	/* only one mmap() call per device for now */
	if (client->buffer.pages)
		return -EAGAIN;	

	ret = buffer_alloc(&client->buffer, page_count);
	if (ret)
		return ret;

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	ret = buffer_map_vma(&client->buffer, vma);
	if (ret)
		goto fail;

	printk(KERN_INFO "uncached ram mmap successful\n");
	return 0;
 fail:
	buffer_destroy(&client->buffer);
	return ret;
}


// Character device file operations
//   http://www.makelinux.net/ldd3/chp-3-sect-3

const struct file_operations device_ops = {
        .owner          = THIS_MODULE,
        .open           = device_op_open,
	.release        = device_op_release,   
        .mmap           = device_op_mmap
};


static int __init uncached_ram_init(void){
   printk(KERN_INFO "Uncached ram module loaded\n");

   cdev_major = register_chrdev(0, "uncached_ram", &device_ops);
   if (cdev_major < 0)
	   return cdev_major;
   printk(KERN_INFO "Created char device, major: %i\n", cdev_major);

   return 0;
}

static void __exit uncached_ram_exit(void){
   unregister_chrdev(cdev_major, "uncached_ram");

   printk(KERN_INFO "Uncached ram module unloaded\n");
}

module_init(uncached_ram_init);
module_exit(uncached_ram_exit);
