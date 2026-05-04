/* skeleton.c */
#include <linux/module.h>   /* needed by all modules */
#include <linux/init.h>     /* needed for macros */
#include <linux/kernel.h>   /* needed for debugging */

#include <linux/moduleparam.h>  /* needed for module parameters */

#include <linux/slab.h>     /* needed for dynamic memory allocation */
#include <linux/list.h>     /* needed for linked list processing */
#include <linux/string.h>   /* needed for string handling */

#include <linux/ioport.h>   /* needed for memory region handling */
#include <linux/io.h>       /* needed for mmio handling */


static struct resource* res=0;
// static struct task_struct* my_thread;

// static int temp_thread (void* data)
// {
//     pr_info ("skeleton thread is now active...\n");
//     while(!kthread_should_stop()) {
//         ssleep (5);
//         pr_info ("skeleton thread is kick every 5 seconds...\n");
//     }
//     return 0;
// }

static int __init skeleton_init(void)
{
    unsigned char* regs={0};
    long temp = 0;

    pr_info ("Linux module CPU temp loaded\n");

    res = request_mem_region (0x01C25000, 0x1000, "allwiner h5 ths");

    if ((res == 0))
        pr_info ("Error while reserving memory region... [0]=%d", res);

    regs = ioremap (0x01C25000, 0x1000);

    if ((regs == 0)) {
        pr_info ("Error while trying to map processor register...\n");
        return -EFAULT;
    }

    temp = -1191 * (int)ioread32(regs+0x80) / 10 + 223000;
    pr_info ("temperature=%ld (%d)\n", temp, ioread32(regs+0x80));


    iounmap (regs);

    return 0;
}

static void __exit skeleton_exit(void)
{
    pr_info ("Linux module skeleton unloaded\n");
    if (res != 0){
        release_mem_region (0x01C25000, 0x1000);
    }
    
}

module_init (skeleton_init);
module_exit (skeleton_exit);

MODULE_AUTHOR ("Daniel Gachet <daniel.gachet@hefr.ch>");
MODULE_DESCRIPTION ("Module skeleton");
MODULE_LICENSE ("GPL");