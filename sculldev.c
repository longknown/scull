#include <linux/capability.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/math64.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
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
    .compat_ioctl          = scull_ioctl,
};

// global simple instance of scull_dev
struct scull_dev gSdev[DEVICE_NUM];

//static
int gScull_major = SCULL_MAJOR, gScull_minor = 0, gDev_nums = DEVICE_NUM;
//static
int gScull_qset = SCULL_SET, gScull_quantum = SCULL_QUANTUM;

module_param(gScull_major, int, S_IRUGO);
module_param(gScull_minor, int, S_IRUGO);
module_param(gDev_nums, int, S_IRUGO);
module_param(gScull_qset, int, S_IRUGO);
module_param(gScull_quantum, int, S_IRUGO);

/*
 * wrap the device number allocation and free
 * return 0 on success
 */
static int alloc_dev_num(void)
{
    dev_t dev;
    int result;

    if(gScull_major) {
        dev = MKDEV(gScull_major, gScull_minor);
        result = register_chrdev_region(dev, gDev_nums, DEVICE_NAME);
    } else {
        result = alloc_chrdev_region(&dev, gScull_minor, gDev_nums, DEVICE_NAME);
        gScull_major = MAJOR(dev);
    }

    if(result < 0) {
        ALOGD("%s: can't get major number %d\n", DEVICE_NAME, gScull_major);
    }
    return result;
}

static void free_dev_num(void)
{
    dev_t dev;
    dev = MKDEV(gScull_major, gScull_minor);
    unregister_chrdev_region(dev, gDev_nums);
}


/*
 * init on the index-th minor device and add the char device to kernel
 */
static int scull_dev_init(struct scull_dev *sdev, int index)
{
    int err;
    dev_t dev = MKDEV(gScull_major, gScull_minor+index);

    cdev_init(&sdev->cdev, &gScull_fops);
    sdev->cdev.owner = THIS_MODULE;
    err = cdev_add(&sdev->cdev, dev, 1);
    if(err) {
        ALOGD("cdev: failed to add cdev to kernel for device(%d, %d), errno=%d\n", \
                gScull_major, gScull_minor+index, err);
    }

    return err;
}

/*
 * a private function dedicated to reset and reallocate qset structure,
 * called on open driver, return 0 on success, -1 on failure
 */
static int scull_resetqset(struct file *filp)
{
    struct scull_dev *sdev = (struct scull_dev *) filp->private_data;
    int err;

    if(down_interruptible(&sdev->sem))
        return -ERESTARTSYS;
    scull_trim(sdev);
    // at least we should allocate the data quantums for the first time
    kfree(sdev->data);
    err = scull_alloc(sdev);
    up(&sdev->sem);
    return err;
}


/* XXX dummy functions for scull_llseek() */
loff_t scull_llseek(struct file *filp, loff_t offset, int i)
{
    return 0;
}


/*
 * our ioctl mainly dedicated to set scull[0-3] qset and quantum params
 * and perform a scull_trim after the successful WRITE cmd
 * our ioctl return non-negative on successful, negative on failure
 */
long scull_ioctl(struct file *filp, unsigned int cmd, unsigned long argp)
{
    int err = 0, retval = 0;
    int tmp;
    struct scull_dev *sdev = (struct scull_dev *) filp->private_data;

    // checking cmd type and NR to assure this is a valid scull cmd
    if(_IOC_TYPE(cmd) != SCULL_IOC_MAGIC) return -ENOTTY;
    if(_IOC_NR(cmd) >= MAXNR) return -ENOTTY;

    // check the validity of user provided data
    if((_IOC_DIR(cmd) & _IOC_READ))
        err = !access_ok(VERIFY_WRITE, (void __user *) argp, _IOC_SIZE(cmd));
    else if((_IOC_DIR(cmd) & _IOC_WRITE))
        err = !access_ok(VERIFY_READ, (void __user*)argp, _IOC_SIZE(cmd));
    else
        err = 0;

    if(err) return -EFAULT;

    switch (_IOC_NR(cmd)) {
        case SQUANTUM:
            if(!capable(CAP_SYS_ADMIN))
                return -EPERM;
            retval = __get_user(tmp, (int __user*)argp);
            // check the user-provided params positive
            if(retval == 0 && tmp <= 0)
                retval = -EFAULT;
            else if(retval == 0) {
                ALOGD("ioctl: set quantum to %d\n", tmp);
                sdev->quantum = tmp;
                retval = scull_resetqset(filp);
            }
			break;
        case SQSET:
            if(!capable(CAP_SYS_ADMIN))
                return -EPERM;
            retval = __get_user(tmp, (int __user*)argp);
            // check the user-provided params positive
            if(retval == 0 && tmp <= 0)
                retval = -EFAULT;
            else if(retval == 0) {
                ALOGD("ioctl: set qset to %d\n", tmp);
                sdev->qset = tmp;
                retval = scull_resetqset(filp);
            }
			break;
        case TQUANTUM:
            if(!capable(CAP_SYS_ADMIN))
                return -EPERM;
            // check the user-provided params positive
            if((tmp = (int)argp) > 0) {
                ALOGD("ioctl: set quantum to %d\n", tmp);
                sdev->quantum = tmp;
                retval = scull_resetqset(filp);
            } else
                retval = -EFAULT;
			break;
        case TQSET:
            if(!capable(CAP_SYS_ADMIN))
                return -EPERM;
            // check the user-provided params positive
            if((tmp = (int)argp) > 0) {
                ALOGD("ioctl: set qset to %d\n", tmp);
                sdev->qset = tmp;
                retval = scull_resetqset(filp);
            } else
                retval = -EFAULT;
			break;
        case GQUANTUM:
            retval = __put_user(sdev->quantum, (int __user*)argp);
			break;
        case GQSET:
            retval = __put_user(sdev->qset, (int __user*)argp);
			break;
        case QQUANTUM:
            retval = sdev->quantum;
			break;
        case QQSET:
            retval = sdev->qset;
			break;
        case XQUANTUM:
            if(!capable(CAP_SYS_ADMIN))
                return -EPERM;
            retval = __get_user(tmp, (int __user*)argp);
            if(retval > 0 && tmp <= 0)
                retval = -EFAULT;
            else if(retval > 0) {
                retval = __put_user(sdev->quantum, (int __user*)argp);
                sdev->quantum = tmp;
                ALOGD("ioctl: set quantum to %d\n", tmp);
                retval = scull_resetqset(filp);
            }
			break;
        case XQSET:
            if(!capable(CAP_SYS_ADMIN))
                return -EPERM;
            retval = __get_user(tmp, (int __user*)argp);
            if(retval > 0 && tmp <= 0)
                retval = -EFAULT;
            else if(retval > 0) {
                retval = __put_user(sdev->qset, (int __user*)argp);
                sdev->qset = tmp;
                ALOGD("ioctl: set qset to %d\n", tmp);
                retval = scull_resetqset(filp);
            }
			break;
        case HQUANTUM:
            if(!capable(CAP_SYS_ADMIN))
                return -EPERM;
            tmp = (int)argp;
            if(tmp <= 0)
                return -EFAULT;
            else {
                retval = sdev->quantum;
                sdev->quantum = tmp;
                ALOGD("ioctl: set quantum to %d\n", tmp);
                retval = scull_resetqset(filp);
            }
			break;
        case HQSET:
            if(!capable(CAP_SYS_ADMIN))
                return -EPERM;
            tmp = (int)argp;
            if(tmp <= 0)
                return -EFAULT;
            else {
                retval = sdev->qset;
                sdev->qset = tmp;
                ALOGD("ioctl: set qset to %d\n", tmp);
                retval = scull_resetqset(filp);
            }
			break;
        case RESET:
            if(!capable(CAP_SYS_ADMIN))
                return -EPERM;
            sdev->qset = gScull_qset;
            ALOGD("ioctl: reset quantm and qset\n");
            sdev->quantum = gScull_quantum;
            retval = scull_resetqset(filp);
            break;
        default:
            retval = -EFAULT;
            break;
    }

    return retval;
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
        return scull_resetqset(filp);
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
        return -1;
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
            *datap = NULL;
        }
        kfree(cur->data);
        cur->data = NULL;
        kfree(cur);
        cur = qp;
    }

    sdev->data = NULL;
    sdev->size = 0UL;
    // set qset and quantum if uninitialised
    sdev->qset = sdev->qset? : gScull_qset;
    sdev->quantum = sdev->quantum? : gScull_quantum;
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
        for(index = 0; index < gDev_nums; ++index) {
            // initialise the struct scull_dev before any file operation
            memset(&gSdev[index], 0, sizeof(struct scull_dev));
            scull_trim(&gSdev[index]);
            sema_init(&gSdev[index].sem, 1);
            sema_init(&gSdev[index].proc_sem, 1);

            err = scull_dev_init(&gSdev[index], index);
            if(err)
                goto fail;
        }

        proc_create("driver/scullproc", 0, NULL, &proc_fops);
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
    for(index = 0; index < gDev_nums; ++index) {
        cdev_del(&(gSdev[index].cdev));
        scull_trim(&gSdev[index]);
    }
    remove_proc_entry("driver/scullproc", NULL);
    ALOGD("scull module removed from kernel!\n");
}

module_init(scull_init);
module_exit(scull_exit);
