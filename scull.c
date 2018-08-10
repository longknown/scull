#include <linux/uaccess.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/math64.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/slab.h>
#include "scull.h"

MODULE_LICENSE("Dual BSD/GPL");

struct file_operations gScull_fops = {
    .owner          = THIS_MODULE,
    .llseek         = scull_llseek,
    .read           = scull_read,
    .write          = scull_write,
    .release        = scull_release,
    .unlocked_ioctl = scull_ioctl,
    .open           = scull_open,
};

// global simple instance of scull_dev
struct scull_dev gSdev[DEVICE_NUM];

static int scull_major = SCULL_MAJOR, scull_minor = 0, scull_nr_devs = DEVICE_NUM;
static int scull_qset = SCULL_SET, scull_quantum = SCULL_QUANTUM;

module_param(scull_major, int, S_IRUGO);
module_param(scull_minor, int, S_IRUGO);
module_param(scull_nr_devs, int, S_IRUGO);
module_param(scull_qset, int, S_IRUGO);
module_param(scull_quantum, int, S_IRUGO);

/*
 * wrap the device number allocation and free
 * return 0 on success
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
        ALOGD("%s: can't get major number %d\n", DEVICE_NAME, scull_major);
    }
    return result;
}

static void free_dev_num(void)
{
    dev_t dev;
    dev = MKDEV(scull_major, scull_minor);
    unregister_chrdev_region(dev, scull_nr_devs);
}


/*
 * init on the index-th minor device and add the char device to kernel
 */
static int scull_dev_init(struct scull_dev *sdev, int index)
{
    int err;
    dev_t dev = MKDEV(scull_major, scull_minor+index);

    cdev_init(&sdev->cdev, &gScull_fops);
    sdev->cdev.owner = THIS_MODULE;
    err = cdev_add(&sdev->cdev, dev, 1);
    if(err) {
        ALOGD("cdev: failed to add cdev to kernel for device(%d, %d), errno=%d\n", \
                scull_major, scull_minor+index, err);
    }

    return err;
}

/* XXX dummy functions for scull_llseek() and scull_ioctl() */
loff_t scull_llseek(struct file *filp, loff_t offset, int i)
{
    return 0;
}


long scull_ioctl(struct file *filp, unsigned int ui, unsigned long ul)
{
    return 0;
}


int scull_open(struct inode *inode, struct file *filp)
{
    struct scull_dev *sdev;

    sdev = container_of(inode->i_cdev, struct scull_dev, cdev);
    filp->private_data = (void *)sdev;

    // trim the device size to 0, when opened in Write-Only mode
    ALOGV("scull_open: calls scull_open with flag 0x%x", filp->f_flags & O_ACCMODE);
    if((filp->f_flags & O_ACCMODE) == O_WRONLY) {
        ALOGV("scull_open: in O_WRONLY mode, trim and re-alloc the data");
        scull_trim(sdev);
        // at least we should allocate the data quantums for the first time
        kfree(sdev->data);
        if(scull_alloc(sdev))
            return -1;
    }

    return 0;
}

int scull_release(struct inode *inode, struct file *filp)
{
    // XXX release nothing right now, be careful scull_release would be called
    //      each time device file is closed
    ALOGV("scull_release: release file\n");
    return 0;
}

int scull_alloc(struct scull_dev *sdev)
{
    if(!sdev->data && !(sdev->data = kmalloc(sizeof(struct scull_qset), GFP_KERNEL))) {
        ALOGD("failed to allocate scull_qset to store data\n");
        return 1;
    }

    sdev->data->data = NULL;
    sdev->data->next = NULL;
    return 0;
}

int scull_trim(struct scull_dev *sdev)
{
    struct scull_qset *root = sdev->data, *qp, *cur;
    void **datap;
    ALOGV("scull_trim: be careful, we are going to trim the data!\n");

    cur = root;
    while(cur) {
        qp = cur->next;
        // release quantum set of the cur scull_qset
        for(datap = cur->data; datap && *datap; ++datap) {
            kfree(*datap);
        }
        kfree(cur->data);
        kfree(cur);
        cur = qp;
    }

    sdev->data = NULL;
    sdev->size = 0UL;
    sdev->qset = scull_qset;
    sdev->quantum = scull_quantum;
    return 0;
}

struct scull_qset *scull_follow(struct scull_dev *dev, int item)
{
    struct scull_qset *qptr = dev->data;
    while(item-- > 0 && qptr) {
        qptr = qptr->next;
    }

    return qptr;
}

ssize_t scull_read(struct file *filp, char __user *buffer, size_t count, loff_t *f_pos)
{
    struct scull_dev *dev = (struct scull_dev *)filp->private_data;
    int quantum = dev->quantum, qset = dev->qset;
    int32_t item_size = quantum * qset; // size of each qset
    int64_t item_n;
    int32_t r_pos, item_r, q_pos;
    struct scull_qset *qptr;
    ssize_t read = 0;
    ALOGV("scull_read: tries to read %d at offset %llu\n", \
            count, *f_pos);

    if(down_interruptible(&dev->sem))
        return -ERESTARTSYS;
    else if(*f_pos > dev->size)
        goto done;
    else if(*f_pos + count > dev->size)
        count = dev->size - *f_pos;

    // we will find the right place to read
    // we have to take care of 64, 32 division and remainder, using <linux/math64.h>
    item_n = div_s64_rem(*f_pos, item_size, &item_r); // which qset

    qptr = scull_follow(dev, item_n);

    // the position at that scull_qset
    q_pos = item_r / quantum;
    r_pos = item_r % quantum;

    // what if the data is damaged?
    if(qptr == NULL || qptr->data == NULL || qptr->data[q_pos] == NULL) {
        ALOGV("scull_read: no available data for the read request\n");
        goto done;
    }
    count = (count > quantum-r_pos) ? quantum-r_pos : count;
    if(copy_to_user(buffer, qptr->data[q_pos]+r_pos, count)) {
        read = -EFAULT;
        goto done;
    }
    *f_pos += count;
    read = count;
    ALOGV("scull_read: successfully read %d from device\n", \
            read);

done:
    up(&dev->sem);
    return read;
}

ssize_t scull_write(struct file *filp, const char __user *buffer, size_t count, loff_t *f_pos)
{
    struct scull_dev *dev = (struct scull_dev *)filp->private_data;
    int quantum, qset;
    struct scull_qset *qptr;
    int64_t item_n;
    int item_r, q_pos, r_pos, item_size;
    ssize_t retval = 0;
    ALOGV("scull_write: tries to write %d at offset %llu\n", \
            count, *f_pos);

    quantum = dev->quantum;
    qset = dev->qset;
    item_size = quantum * qset;

    if(down_interruptible(&dev->sem))
        return -ERESTARTSYS;

    item_n = div_s64_rem(*f_pos, item_size, &item_r); // which qset

    qptr = scull_follow(dev, item_n);
    if(!qptr)
        goto done;

    if(!qptr->data) {
        qptr->data = kmalloc(qset * sizeof(void *), GFP_KERNEL);
        if(!qptr->data)
            goto done;
        memset(qptr->data, 0, qset * sizeof(void *));
    }
    q_pos = item_r / quantum;
    r_pos = item_r % quantum;

    if(!qptr->data[q_pos]) {
        qptr->data[q_pos] = kmalloc(quantum, GFP_KERNEL);
        if(!qptr->data[q_pos])
            goto done;
    }
    count = (count > quantum-r_pos)? quantum-r_pos : count;
    if(copy_from_user(qptr->data[q_pos]+r_pos, buffer, count)) {
        retval = -EFAULT;
        goto done;
    }

    *f_pos += count;
    retval = count;
    ALOGV("scull_write: successfully write %d to device\n", \
            retval);
    if(dev->size < *f_pos)  // f_pos may be llseek to be behind the device size
        dev->size = *f_pos;

done:
    up(&dev->sem);
    return retval;
}

int __init scull_init(void)
{
    int index;
    int err;
    if((err=alloc_dev_num()) == 0) {
        // register 4 devices
        for(index = 0; index < DEVICE_NUM; ++index) {
            // initialise the struct scull_dev before any file operation
            memset(&gSdev[index], 0, sizeof(struct scull_dev));
            scull_trim(&gSdev[index]);
            sema_init(&gSdev[index].sem, 1);

            err = scull_dev_init(&gSdev[index], index);
            if(err)
                goto fail;
        }
        ALOGD("scull module inserted to kernel!\n");
    }

    return err;

fail:
    // unroll the cdev_add()
    while(index-- > 0) {
        cdev_del(&(gSdev[index].cdev));
    }
    free_dev_num();
    return err;
}

void __exit scull_exit(void)
{
    int index;
    
    free_dev_num();
    for(index = 0; index < DEVICE_NUM; ++index) {
        cdev_del(&(gSdev[index].cdev));
        scull_trim(&gSdev[index]);
    }
    ALOGD("scull module removed from kernel!\n");
}

module_init(scull_init);
module_exit(scull_exit);
