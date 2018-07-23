#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/types.h>
#include <linux/slab.h>
#include "scull.h"

MODULE_LICENSE("Dual BSD/GPL");

static int scull_major = SCULL_MAJOR, scull_minor = 0, scull_nr_devs = DEVICE_NUM;
static int scull_set = SCULL_SET, scull_quantum = SCULL_QUANTUM;

module_param(scull_major, int, S_IRUGO);
module_param(scull_minor, int, S_IRUGO);
module_param(scull_nr_devs, int, S_IRUGO);
module_param(scull_set, int, S_IRUGO);
module_param(scull_quantum, int, S_IRUGO);

/*
 * wrap the device number allocation and free
 */
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

struct file_operations scull_fops = {
    .owner          = THIS_MODULE,
    .llseek         = scull_llseek,
    .read           = scull_read,
    .write          = scull_write,
    .release        = scull_release,
    .unlocked_ioctl = scull_ioctl,
    .open           = scull_open,
};

/*
 * init on the index-th minor device and add the char device to kernel
 */
static void scull_dev_init(struct scull_dev *sdev, int index)
{
    int err;
    dev_t dev = MKDEV(scull_major, scull_minor+index);

    cdev_init(&sdev->cdev, &scull_fops);
    sdev->cdev->owner = THIS_MODULE;
    err = cdev_add(&sdev->cdev, dev, 1);
    if(err < 0) {
        printk(KERN_ALERT "cdev: failed to add cdev to kernel for device(%d, %d)", \
                scull_major, scull_minor+index);
    }
}


int scull_open(struct inode *inode, struct file *filp)
{
    struct scull_dev *sdev;
    sdev = container_of(inode->i_cdev, struct scull_dev, cdev);
    filp->private_data = (void *)sdev;

    // trim the device size to 0, when opened in Write-Only mode
    if((filp->f_flags & O_ACCMODE) == O_WRONLY) {
        scull_trim(sdev);
    }

    return 0;
}

int scull_release(struct inode *inode, struct file *filp)
{
    // XXX currently nothing needed to be handled here
    return 0;
}

int scull_trim(struct scull_dev *sdev)
{
    struct scull_qset *root = sdev->qset, *qp, *current;
    void **datap;

    current = root;
    while(current) {
        qp = current->next;
        // release quantum set of the current scull_qset
        for(datap = current->data; datap && *datap; ++datap) {
            kfree(*datap);
        }
        kfree(current->data);
        kfree(current);
        current = qp;
    }

    sdev->qset = NULL;
    sdev->size = 0UL;
    sdev->qset = scull_qset;
    sdev->quantum = scull_quantum;
    return 0;
}
