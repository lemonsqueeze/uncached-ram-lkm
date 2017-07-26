/**
 * @file    hello.c
 * @author  Derek Molloy
 * @date    4 April 2015
 * @version 0.1
 * @brief  An introductory "Hello World!" loadable kernel module (LKM) that can display a message
 * in the /var/log/kern.log file when the module is loaded and removed. The module can accept an
 * argument when it is loaded -- the name, which appears in the kernel log files.
 * @see http://www.derekmolloy.ie/ for a full description and follow-up descriptions.
*/

#include <linux/init.h>             // Macros used to mark up functions e.g., __init __exit
#include <linux/module.h>           // Core header for loading LKMs into the kernel
#include <linux/kernel.h>           // Contains types, macros, functions for the kernel

#include <linux/page-flags.h>
#include <asm/cacheflush.h>

#include <linux/device.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/slab.h>

MODULE_LICENSE("GPL");              ///< The license type -- this affects runtime behavior
MODULE_AUTHOR("Derek Molloy");      ///< The author -- visible when you use modinfo
MODULE_DESCRIPTION("A simple Linux driver for the BBB.");  ///< The description -- see modinfo
MODULE_VERSION("0.1");              ///< The version of the module

static char *name = "world";        ///< An example LKM argument -- default value is "world"
module_param(name, charp, S_IRUGO); ///< Param desc. charp = char ptr, S_IRUGO can be read/not changed
MODULE_PARM_DESC(name, "The name to display in /var/log/kern.log");  ///< parameter description

struct fw_iso_buffer {
        char **pages;
        int page_count;
};

struct client {
	struct fw_iso_buffer buffer;
	unsigned long vm_start;
};

static void fw_iso_buffer_destroy(struct fw_iso_buffer *buffer)
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


static int fw_iso_buffer_alloc(struct fw_iso_buffer *buffer, int page_count)
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
		fw_iso_buffer_destroy(buffer);
		return -ENOMEM;
	}

	return 0;
}


static int fw_iso_buffer_map_vma(struct fw_iso_buffer *buffer,
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


int hello_cdev_major;

static int hello_device_op_open(struct inode *inode, struct file *file)
{
	struct client *client;

	client = kzalloc(sizeof(*client), GFP_KERNEL);
	if (client == NULL)
		return -ENOMEM;

	file->private_data = client;

	return nonseekable_open(inode, file);
}

static int hello_device_op_release(struct inode *inode, struct file *file)
{
	struct client *client = file->private_data;

	if (client->buffer.pages)
		fw_iso_buffer_destroy(&client->buffer);

	kfree(client);	
	return 0;
}


static int hello_device_op_mmap(struct file *file, struct vm_area_struct *vma)
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

	ret = fw_iso_buffer_alloc(&client->buffer, page_count);
	if (ret < 0)
		return ret;

	ret = fw_iso_buffer_map_vma(&client->buffer, vma);
	if (ret < 0)
		goto fail;
	printk(KERN_INFO "mmap successful\n");

	return 0;
 fail:
	fw_iso_buffer_destroy(&client->buffer);
	return ret;
}


// Character device file operations
//   http://www.makelinux.net/ldd3/chp-3-sect-3

const struct file_operations hello_device_ops = {
        .owner          = THIS_MODULE,
        .open           = hello_device_op_open,
	.release        = hello_device_op_release,   
        .mmap           = hello_device_op_mmap
};



/** @brief The LKM initialization function
 *  The static keyword restricts the visibility of the function to within this C file. The __init
 *  macro means that for a built-in driver (not a LKM) the function is only used at initialization
 *  time and that it can be discarded and its memory freed up after that point.
 *  @return returns 0 if successful
 */
static int __init helloBBB_init(void){
   printk(KERN_INFO "EBB: Hello %s from the BBB LKM!\n", name);

   hello_cdev_major = register_chrdev(0, "hello", &hello_device_ops);
   if (hello_cdev_major < 0)
	   return hello_cdev_major;
   printk(KERN_INFO "Created char device, major: %i\n", hello_cdev_major);

   return 0;
}

/** @brief The LKM cleanup function
 *  Similar to the initialization function, it is static. The __exit macro notifies that if this
 *  code is used for a built-in driver (not a LKM) that this function is not required.
 */
static void __exit helloBBB_exit(void){
   unregister_chrdev(hello_cdev_major, "hello");

   printk(KERN_INFO "EBB: Goodbye %s from the BBB LKM!\n", name);
}

/** @brief A module must use the module_init() module_exit() macros from linux/init.h, which
 *  identify the initialization function at insertion time and the cleanup function (as
 *  listed above)
 */
module_init(helloBBB_init);
module_exit(helloBBB_exit);
