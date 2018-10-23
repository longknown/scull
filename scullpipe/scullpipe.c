#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include "scullpipe.h"

MODULE_LICENSE("Dual BSD/GPL");

// only visible in module init and exit
static struct scullp_cdev gscullp_dev;
static int gbufsize = BUFSIZE;
static unsigned int gmajor, gminor;
module_param(gbufsize, int, S_IRUGO);

static struct file_operations scullp_fops = {
    .owner          = THIS_MODULE,
    .open           = scullp_open,
    .read           = scullp_read,
    .write          = scullp_write,
    .compat_ioctl   = scullp_ioctl,
    .llseek         = scullp_llseek,
    .release        = scullp_release,
};

static int __init scullp_init(void)
{
    dev_t devt;
    int err;

    // allocate device number
    err = alloc_chrdev_region(&devt, 0, 1, DEV_NAME);
    if(err < 0) {
        ALOGD("error: failed to allocate device number");
        return err;
    }
    gmajor = MAJOR(devt);
    gminor = MINOR(devt);

    // register char device
    cdev_init(&gscullp_dev.cdev, &scullp_fops);
    
    gscullp_dev.ops = &scullp_fops;
    err = cdev_add(&gscullp_dev, devt, 1);

    if(err) {
        ALOGD("error: unable to register char device to kernel");
        goto fail;
    }
    return err;

fail:
    unregister_chrdev_region(devt, 1);
    return err;
}

static void __exit scullp_exit(void)
{
    dev_t devt = MKDEV(gmajor, gminor);

    cdev_del(&gscullp_dev.cdev);
    unregister_chrdev_region(devt, 1);
}


module_init(scullp_init);
module_exit(scullp_exit);
