/* skeleton.c */
#include <linux/delay.h>  /* needed for delay fonctions */
#include <linux/device.h> /* needed for sysfs handling */
#include <linux/gpio.h>
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
#include <linux/timer.h>

#define CLASS

#define GPIO_LED_FREQ 10

struct module_config {
    char mode[30];
    int frequency;
};

static struct module_config config;
struct thermal_zone_device* thermal_zone;
static struct task_struct* my_thread;
static struct timer_list my_timer;

static const char module_name[] = "my_module";

static int temp;

ssize_t temp_show(struct device* dev, struct device_attribute* attr,
                  char* buf) {
    sprintf(buf, "%d\n", temp);
    return strlen(buf);
}
DEVICE_ATTR_RO(temp);

ssize_t config_show(struct device* dev, struct device_attribute* attr,
                    char* buf) {
    sprintf(buf, "%s %d\n", config.mode,config.frequency);
    return strlen(buf);
}
ssize_t config_store(struct device* dev, struct device_attribute* attr,
                     const char* buf, size_t count) {
    sscanf(buf, "%s %d", &config.mode, config.frequency);
    return count;
}
DEVICE_ATTR(config, 0664, config_show, config_store);

static struct class* sysfs_class;
static struct device* sysfs_device;

int get_temp(void) {
    static bool led = 0;
    int t = 0;
    int ret = thermal_zone_get_temp(thermal_zone, &t);
    if (ret == 0) {
        pr_info("Température CPU : %d.%d °C\n", t / 1000, t % 1000);
        gpio_set_value(GPIO_LED_FREQ, led = !led);
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

static void my_timer_callback(struct timer_list* timer) {
    pr_info("Timer callback called (%ld)\n", jiffies);
    mod_timer(&my_timer, jiffies + msecs_to_jiffies(2000));
}

void deinit_gpio(void) { gpio_free(GPIO_LED_FREQ); }

static int __init skeleton_init(void) {
    int ret = 0;
    pr_info("Linux module CPU temp loaded\n");

    thermal_zone = thermal_zone_get_zone_by_name("cpu-thermal");
    if (!thermal_zone) {
        pr_err("Failed to get thermal zone 0\n");
        return -ENODEV;
    }

    my_thread = kthread_run(temp_thread, 0, "s/thread");

    // --------------  SYSFS 
    int status = 0;
    sysfs_class = class_create(THIS_MODULE, module_name);
    sysfs_device = device_create(sysfs_class, NULL, 0, NULL, module_name);
    if (status == 0) status = device_create_file(sysfs_device, &dev_attr_temp);
    if (status == 0)
        status = device_create_file(sysfs_device, &dev_attr_config);


    // --------------  GPIO
    ret = gpio_request(GPIO_LED_FREQ, "gpio_led_freq");
    if (ret < 0) {
        pr_err("Failed to get gpio led freq\n");
        return ret;
    }
    ret = gpio_direction_output(GPIO_LED_FREQ, 0);
    if (ret < 0) {
        pr_err("Failed to set output gpio led freq\n");
        gpio_free(GPIO_LED_FREQ);
        return ret;
    }

    // -------------- Timer
    timer_setup(&my_timer, my_timer_callback, 0);
    mod_timer(&my_timer, jiffies + msecs_to_jiffies(2000));

    return 0;
}

static void __exit skeleton_exit(void) {
    pr_info("Linux module skeleton unloaded\n");
    kthread_stop(my_thread);

    device_remove_file(sysfs_device, &dev_attr_temp);
    device_remove_file(sysfs_device, &dev_attr_config);
    device_destroy(sysfs_class, 0);
    class_destroy(sysfs_class);

    deinit_gpio();

    del_timer(&my_timer);
}

module_init(skeleton_init);
module_exit(skeleton_exit);

MODULE_AUTHOR("Nicolas Rivier <nicolas.rivier@hes-so.ch>");
MODULE_DESCRIPTION("Module");
MODULE_LICENSE("GPL");