/* skeleton.c */
#include <linux/delay.h>       /* needed for delay fonctions */
#include <linux/device.h>      /* needed for sysfs handling */
#include <linux/init.h>        /* needed for macros */
#include <linux/io.h>          /* needed for mmio handling */
#include <linux/ioport.h>      /* needed for memory region handling */
#include <linux/kernel.h>      /* needed for debugging */
#include <linux/kthread.h>     /* needed for kernel thread management */
#include <linux/list.h>        /* needed for linked list processing */
#include <linux/module.h>      /* needed by all modules */
#include <linux/moduleparam.h> /* needed for module parameters */
#include <linux/slab.h>        /* needed for dynamic memory allocation */
#include <linux/string.h>      /* needed for string handling */
#include <linux/thermal.h>

#define CLASS

struct module_config {
    char mode[30];
};

static struct module_config config;
struct thermal_zone_device* thermal_zone;
static struct task_struct* my_thread;

static int temp;

ssize_t sysfs_show_val(struct device* dev, struct device_attribute* attr,
                       char* buf) {
    sprintf(buf, "%d\n", temp);
    return strlen(buf);
}
ssize_t sysfs_store_val(struct device* dev, struct device_attribute* attr,
                        const char* buf, size_t count) {
    temp = simple_strtol(buf, 0, 10);
    return count;
}
DEVICE_ATTR(temp, 0664, sysfs_show_val, sysfs_store_val);

#ifdef CLASS
static struct class* sysfs_class;
static struct device* sysfs_device;
#endif

int get_temp(void) {
    int t = 0;
    int ret = thermal_zone_get_temp(thermal_zone, &t);
    if (ret == 0) {
        pr_info("Température CPU : %d.%d °C\n", t / 1000, t % 1000);
    } else {
        pr_info("Error while getting temp...\n");
        return -1;
    }
    return t;
}

static int temp_thread(void* data) {
    pr_info("temp thread is now active...\n");
    while (!kthread_should_stop()) {
        ssleep(5);
        temp = get_temp();
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

    my_thread = kthread_run(temp_thread, 0, "s/thread");

    int status = 0;
#ifdef CLASS
    sysfs_class = class_create(THIS_MODULE, "my_module_sysfs_class");
    sysfs_device =
        device_create(sysfs_class, NULL, 0, NULL, "my_module_sysfs_class");
    if (status == 0) status = device_create_file(sysfs_device, &dev_attr_temp);
#endif

    return 0;
}

static void __exit skeleton_exit(void) {
    pr_info("Linux module skeleton unloaded\n");
    kthread_stop(my_thread);

#ifdef CLASS
    device_remove_file(sysfs_device, &dev_attr_temp);
    device_destroy(sysfs_class, 0);
    class_destroy(sysfs_class);
#endif
}

module_init(skeleton_init);
module_exit(skeleton_exit);

MODULE_AUTHOR("Nicolas Rivier <nicolas.rivier@hes-so.ch>");
MODULE_DESCRIPTION("Module");
MODULE_LICENSE("GPL");