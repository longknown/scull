#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/types.h>
#include "scull.h"

MODULE_LICENSE("Dual BSD/GPL");

static int scull_major = SCULL_MAJOR, scull_minor = 0, scull_nr_devs = DEVICE_NUM;

module_param(scull_major, int, S_IRUGO);
module_param(scull_minor, int, S_IRUGO);
module_param(scull_nr_devs, int, S_IRUGO);

// wrap the device number allocation and free
static int alloc_dev_num(void)
{
    dev_t dev;
    int result;

    if(scull_major) {
        dev = MKDEV(scull_major, scull_minor);
        result = register_chrdev_region(dev, scull_nr_devs, DEVICE_NAME);
    } else {
        result = alloc_chrdev_region(&dev, scull_minor, scull_nr_devs, DEVICE_NAME);
        scull_major = MAJOR(dev);
    }

    if(result < 0) {
        printk(KERN_WARNING "%s: can't get major number %d\n", DEVICE_NAME, scull_major);
    }
    return result
}

static void free_dev_num(void)
{
    dev_t dev;
    dev = MKDEV(scull_major, scull_minor);
    unregister_chrdev_region(dev, scull_nr_devs);
}
