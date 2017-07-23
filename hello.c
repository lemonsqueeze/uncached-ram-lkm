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

MODULE_LICENSE("GPL");              ///< The license type -- this affects runtime behavior
MODULE_AUTHOR("Derek Molloy");      ///< The author -- visible when you use modinfo
MODULE_DESCRIPTION("A simple Linux driver for the BBB.");  ///< The description -- see modinfo
MODULE_VERSION("0.1");              ///< The version of the module

static char *name = "world";        ///< An example LKM argument -- default value is "world"
module_param(name, charp, S_IRUGO); ///< Param desc. charp = char ptr, S_IRUGO can be read/not changed
MODULE_PARM_DESC(name, "The name to display in /var/log/kern.log");  ///< parameter description

#if 0
// x86 only ?
#define alloc_gatt_pages(order)         \
        ((char *)__get_free_pages(GFP_KERNEL, (order)))
#define free_gatt_pages(table, order)   \
        free_pages((unsigned long)(table), (order))
#endif

// use vmalloc()

static int page_order = 10;  // we allocate (1<<page_order) pages
static char *table = 0;

static void
hello_alloc_pages(void)
{
	struct page *page;
	char *table_end;

	table = (char*) __get_free_pages(GFP_KERNEL, page_order);
	if (!table) {  printk(KERN_WARNING "Could not allocate pages\n"); return;  }

	printk(KERN_WARNING "sizeof(phys_addr_t): %i\n", sizeof(phys_addr_t));
	if (sizeof(phys_addr_t) == 8)
		printk(KERN_WARNING "Allocated %li bytes (%i pages) at 0x%llx\n",
		       (1 << page_order) * PAGE_SIZE, 1 << page_order, virt_to_phys(table));
	
	table_end = table + ((PAGE_SIZE * (1 << page_order)) - 1);   

	for (page = virt_to_page(table); page <= virt_to_page(table_end); page++)
                SetPageReserved(page);

	*((int*)table) = 0xdeadbeaf;

	if (set_memory_uc((unsigned long)table, 1 << page_order))
                printk(KERN_WARNING "Could not set pages to UC!\n");
	else
		printk(KERN_WARNING "Success !\n");
}

static void
hello_free_pages(void)
{	
	char *table_end = table + ((PAGE_SIZE * (1 << page_order)) - 1);
	struct page *page;

	printk(KERN_WARNING "Freeing pages\n");
	
	set_memory_wb((unsigned long)table, 1 << page_order);

	for (page = virt_to_page(table); page <= virt_to_page(table_end); page++)
		ClearPageReserved(page);

	free_pages((unsigned long)(table), (page_order));
}


/** @brief The LKM initialization function
 *  The static keyword restricts the visibility of the function to within this C file. The __init
 *  macro means that for a built-in driver (not a LKM) the function is only used at initialization
 *  time and that it can be discarded and its memory freed up after that point.
 *  @return returns 0 if successful
 */
static int __init helloBBB_init(void){
   printk(KERN_INFO "EBB: Hello %s from the BBB LKM!\n", name);
   hello_alloc_pages();
   return 0;
}

/** @brief The LKM cleanup function
 *  Similar to the initialization function, it is static. The __exit macro notifies that if this
 *  code is used for a built-in driver (not a LKM) that this function is not required.
 */
static void __exit helloBBB_exit(void){
   if (table)
	   hello_free_pages();
   printk(KERN_INFO "EBB: Goodbye %s from the BBB LKM!\n", name);
}

/** @brief A module must use the module_init() module_exit() macros from linux/init.h, which
 *  identify the initialization function at insertion time and the cleanup function (as
 *  listed above)
 */
module_init(helloBBB_init);
module_exit(helloBBB_exit);
