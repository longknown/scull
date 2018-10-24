#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include "scullpipe.h"

/*
 * return currently availble bufsize for writers
 * 0 if non is available
 *
 * Reading and writing ptr would rewound back to beginning if it reaches the end,
 * only (bufsize-1) space would be available for writer, so:
 * a) if wp==rp, this is an empty buffer;
 * b) if rp=wp+1, buffer is fully filled;
 *
 * so return value would be as follow:
 * 1) if wp >= rp, return min(rp+bufsize-wp-1, buf_end-wp)
 * 2) if rp > wp, return rp-wp-1;
 *
 */
size_t writerspace_avail(struct scullp_cdev *sdev)
{
    if(sdev->wp >= sdev->rp)
        return min(sdev->rp + sdev->bufsize - sdev->wp - 1, sdev->buf_end - sdev->wp);
    else
        return (sdev->rp - sdev->wp - 1);
}

loff_t scullp_llseek(struct file* filp, loff_t loff, int whence)
{
    return 0;
}

ssize_t scullp_read(struct file* filp, char __user *buf, size_t count, loff_t *f_pos)
{
    struct scullp_cdev *sdev = filp->private_data;
    ssize_t rcount;
    int err;
    wait_queue_entry_t read_wait;

    if(down_interruptible(&sdev->sem))
        return -ERESTARTSYS;

    // wait till writer fill data to buffer
    while(sdev->rp == sdev->wp) {
        // unlock to let writer in
        up(&sdev->sem);
        if(filp->f_flags & O_NONBLOCK)
            return -EAGAIN;
        init_wait(&read_wait);
        ALOGD("%s: proc reading is going to sleep...", current->comm);
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
        rcount = min(count, (size_t)(sdev->wp - sdev->rp));
    else
        rcount = min(count, (size_t)(sdev->buf_end - sdev->rp));
    ALOGV("reading: now we have %lu bytes buffer available to read", rcount);

    if(copy_to_user(buf, sdev->rp, rcount)) {
        err = -EFAULT;
        goto done;
    }
    sdev->rp += rcount;
    // wrap reader ptr back to the beginning
    if(sdev->rp == sdev->buf_end)
        sdev->rp = sdev->buf_begin;
    err = rcount;
    *f_pos += rcount;
    ALOGD("%s: did read %li bytes", current->comm, rcount);
    // wake up sleeping writers
    wake_up_interruptible(&sdev->outq);

done:
    up(&sdev->sem);
    return err;
}

ssize_t scullp_write(struct file* filp, const char __user *buf, size_t count, loff_t *f_pos)
{
    // write method should not wrap write ptr when it reaches the end,
    // otherwise, writing a full-size buffer would be taken as empty buffer
    struct scullp_cdev *sdev = filp->private_data;
    size_t wcount;
    int err;
    wait_queue_entry_t write_wait;

    if(down_interruptible(&sdev->sem))
        return -ERESTARTSYS;

    while((wcount=writerspace_avail(sdev)) == 0) {
        up(&sdev->sem);
        if(filp->f_flags & O_NONBLOCK)
            return -EAGAIN;

        init_wait(&write_wait);
        prepare_to_wait(&sdev->outq, &write_wait, TASK_INTERRUPTIBLE);
        ALOGD("%s: proc writing is going to sleep...", current->comm);
        if((wcount=writerspace_avail(sdev)) == 0)
            schedule();
        finish_wait(&sdev->outq, &write_wait);
        if(signal_pending(current))
            return -ERESTARTSYS;
        if(down_interruptible(&sdev->sem))
            return -ERESTARTSYS;
    }
    wcount = min(wcount, count);
    // wrap around before writing buffer
    ALOGV("writing: now we have %lu bytes of free space to write", wcount);

    if(copy_from_user(sdev->wp, buf, wcount)) {
        err = -EFAULT;
        goto done;
    }
    sdev->wp += wcount;
    f_pos += wcount;
    err = wcount;
    if(sdev->wp == sdev->buf_end)
        sdev->wp = sdev->buf_begin;
    ALOGD("%s: did write %li bytes", current->comm, wcount);

    wake_up_interruptible(&sdev->inq);

done:
    up(&sdev->sem);
    return err;
}

long scullp_ioctl(struct file* filp, unsigned int cmd, unsigned long argp)
{
    return 0;
}

int scullp_open(struct inode* inode, struct file* filp)
{
    struct scullp_cdev *sdev;
    void *ptr;

    sdev = container_of(inode->i_cdev, struct scullp_cdev, cdev);
    filp->private_data = sdev;

    if(down_interruptible(&sdev->sem))
        return -ERESTARTSYS;
    // initialise buffer for the first time
    if(!sdev->buf_begin) {
        ptr = kmalloc(sdev->bufsize, GFP_KERNEL);
        if(!ptr) {
            ALOGD("error: unable to allocate buffer memory!");
            return -ENOMEM;
        } else {
            ALOGV("size(%lu) buffer is allocated.", sdev->bufsize);
            sdev->rp = sdev->wp = sdev->buf_begin = ptr;
            sdev->buf_end = sdev->buf_begin + sdev->bufsize;
        }
    }
    ALOGV("device opened successfully with flags(0x%x)", filp->f_flags);

    up(&sdev->sem);
    return 0;
}

int scullp_release(struct inode *inode, struct file *filp)
{
    return 0;
}
