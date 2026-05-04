/* skeleton.c */
#include <linux/init.h>        /* needed for macros */
#include <linux/io.h>          /* needed for mmio handling */
#include <linux/ioport.h>      /* needed for memory region handling */
#include <linux/kernel.h>      /* needed for debugging */
#include <linux/list.h>        /* needed for linked list processing */
#include <linux/module.h>      /* needed by all modules */
#include <linux/moduleparam.h> /* needed for module parameters */
#include <linux/slab.h>        /* needed for dynamic memory allocation */
#include <linux/string.h>      /* needed for string handling */
#include <linux/thermal.h>
#include <linux/kthread.h>  /* needed for kernel thread management */
#include <linux/delay.h>    /* needed for delay fonctions */
#include <linux/device.h> /* needed for sysfs handling */



#define CLASS

struct module_config {
    int id;
    long ref;
    char name[30];
    char descr[30];
};

static struct skeleton_config config;
struct thermal_zone_device* thermal_zone;
static struct task_struct* my_thread;


int get_temp(void){
    int temp=0;
    int ret = thermal_zone_get_temp(thermal_zone, &temp);
    if (ret == 0) {
        pr_info("Température CPU : %d.%d °C\n", temp / 1000, temp % 1000);
    } else {
        pr_info("Error while getting temp...\n");
        return -1;
    }
    return temp;
}

static int temp_thread (void* data)
{
    pr_info ("temp thread is now active...\n");
    while(!kthread_should_stop()) {
        ssleep (5);
        get_temp();
    }
    return 0;
}

static int __init skeleton_init(void) {
    pr_info("Linux module CPU temp loaded\n");

    thermal_zone = thermal_zone_get_zone_by_name("cpu-thermal");
    if (!thermal_zone) {
        pr_err("Failed to get thermal zone 0\n");
        return -ENODEV;
    }

    my_thread = kthread_run (temp_thread, 0, "s/thread");
    


    return 0;
}

static void __exit skeleton_exit(void) {
    pr_info("Linux module skeleton unloaded\n");
    kthread_stop (my_thread);
}

module_init(skeleton_init);
module_exit(skeleton_exit);

MODULE_AUTHOR("Daniel Gachet <daniel.gachet@hefr.ch>");
MODULE_DESCRIPTION("Module skeleton");
MODULE_LICENSE("GPL");