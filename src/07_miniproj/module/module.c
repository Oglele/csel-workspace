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
#include <linux/minmax.h>

#define CLASS

#define GPIO_LED_FREQ 10

#define FREQ_MAX 20
#define FREQ_MIN 1

enum modes{
    MODE_AUTO,
    MODE_MANUAL
};

struct module_config {
    enum modes mode;
    int frequency;
    int duty;
};

static const char *const str_mode_option[] = {
    "auto", "manual", NULL
};

struct temp_freq_step{
    int temp_threshold;
    int frequency;
};

static const struct temp_freq_step lut_thermal[] = {
    {45000, 20},
    {40000, 10},
    {35000, 5},
    {0, 2}
};
static int lut_size = ARRAY_SIZE(lut_thermal);



static struct module_config config = {
    .mode = MODE_AUTO,
    .frequency = 5,
    .duty = 50
};

struct thermal_zone_device* thermal_zone;
static struct task_struct* my_thread;
static struct timer_list my_timer;

static struct class* sysfs_class;
static struct device* sysfs_device;

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
    sprintf(buf, "%s %d %d\n", str_mode_option[config.mode],config.frequency,config.duty);
    return strlen(buf);
}
ssize_t config_store(struct device* dev, struct device_attribute* attr,
                     const char* buf, size_t count) {
    struct module_config tmp;
    int ret;
    char tmp_buf[10];
    ret = sscanf(buf, "%9s %d %d", tmp_buf, &tmp.frequency, &tmp.duty);
    if (ret != 3) {
        return -EINVAL;
    }
    int idx = sysfs_match_string(str_mode_option, tmp_buf);
    if (idx < 0){
        pr_err("Mode invalide : 'auto' ou 'manual' uniquement\n");
        return -EINVAL;
    }
    config.mode = (enum modes)idx;
    config.frequency = clamp_t(int,tmp.frequency,FREQ_MIN,FREQ_MAX);
    config.duty = clamp_t(int,tmp.duty,0,100);
    return count;
}
DEVICE_ATTR(config, 0664, config_show, config_store);


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
        if (config.mode == MODE_AUTO)
        {
            int tmp_freq;
            int i;
            for (i = 0; i < lut_size; i++)
            {
                tmp_freq = lut_thermal[i].frequency;
                if(temp >= lut_thermal[i].temp_threshold)
                {
                    break;
                }
            }
            config.frequency = tmp_freq;
        }
        
    }
    return 0;
}

static void my_timer_callback(struct timer_list* timer) {
    static bool led = false;
    // pr_info("timer callback led\n");
    int p = 1000 / config.frequency;
    int p1 = p * config.duty / 100;
    int p2 = p - p1; 
    if(led)
    {
        gpio_set_value(GPIO_LED_FREQ, 0);
        mod_timer(&my_timer, jiffies + msecs_to_jiffies(p1));
    }
    else{
        gpio_set_value(GPIO_LED_FREQ, 1);
        mod_timer(&my_timer, jiffies + msecs_to_jiffies(p2));
    }
    led = !led;
    
}

void deinit_gpio(void) { gpio_free(GPIO_LED_FREQ); }

static int __init skeleton_init(void) {
    int ret = 0;
    pr_info("Linux module CPU temp loaded\n");

    // --------------  THERMAL CPU 
    thermal_zone = thermal_zone_get_zone_by_name("cpu-thermal");
    if (!thermal_zone) {
        pr_err("Failed to get thermal zone 0\n");
        return -ENODEV;
    }


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

    my_thread = kthread_run(temp_thread, 0, "s/thread");

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

    del_timer_sync(&my_timer);
}

module_init(skeleton_init);
module_exit(skeleton_exit);

MODULE_AUTHOR("Nicolas Rivier <nicolas.rivier@hes-so.ch>");
MODULE_DESCRIPTION("Module");
MODULE_LICENSE("GPL");