#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include "scullpipe.h"

#define min(a, b) (((a) > (b))? (b) : (a))

loff_t scullp_llseek(struct file* filp, loff_t loff, int whence)
{
}

ssize_t scullp_read(struct file* filp, char __user *buf, size_t count, loff_t *loff)
{
    struct scullp_cdev *sdev = filp->private_data;
    int rcount, err;

    if(down_interruptible(&sdev->sem))
        return -ERESTARTSYS;

    // wait till writer fill data to buffer
    while(sdev->rp == sdev->wp) {
        // unlock to let writer in
        up(&sdev->sem);
        if(filp->f_flags & O_NONBLOCK)
            return -EAGAIN;
        DEFINE_WAIT(read_wait);
        prepare_to_wait(&sdev->inq, &read_wait, TASK_INTERRUPTIBLE);

        if(sdev->rp == sdev->wp)
            schedule();
        finish_wait(&sdev->inq, &read_wait);
        if(signal_pending(current))
            return -ERESTARTSYS;
        if(down_interruptible(&sdev->sem))
            return -ERESTARTSYS;
    }
    if(sdev->wp > sdev->rp)
        rcount = min(count, sdev->wp - sdev->rp);
    else
        rcount = min(count, sdev->buf_end - sdev->rp);

    if(copy_to_user(buf, sdev->rp, rcount)) {
        err = -EFAULT;
        goto done;
    }
    sdev->rp += rcount;
    if(sdev->rp == sdev->buf_end)
        sdev->rp = sdev->buf_begin;
    err = rcount;
    // wake up sleeping writers
    wake_up_interruptible(&sdev->outq);

done:
    up(&sdev->sem);
    return err;
}

ssize_t scullp_write(struct file* filp, const char __user *buf, size_t count, loff_t *loff)
{

}

int scullp_ioctl(struct inode* inode, struct file* filp, unsigned int cmd, unsigned long argp)
{
}

int scullp_open(struct inode* inode, struct file* filp)
{
    struct scullp_cdev *sdev;
    void *ptr;

    sdev = container_of(inode->i_cdev, struct scullp_cdev, cdev);
    filp->private_data = sdev;

    init_MUTEX(&sdev->sem);
    if(down_interruptible(&sdev->sem))
        return -ERESTARTSYS;
    // initialise buffer for the first time
    if(!sdev->buf_begin) {
        ptr = kmalloc(sdev->bufsize, GFP_KERNEL);
        if(!ptr) {
            ALOGD("error: unable to allocate buffer memory!");
            return -ENOMEM;
        } else {
            sdev->rp = sdev->wp = sdev->buf_begin = ptr;
            sdev->buf_end = sdev->buf_begin + sdev->bufsize;
        }
    }

    up(&sdev->sem);
    return 0;
}

int scull_release(struct inode* inode, struct file* filp)
{
    return 0;
}
