/*
 * source file dedicated to /proc/ file creating and implementation
 */
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include "scull.h"

#define BUFSIZE 512
#undef min
#define min(a, b) ((a) > (b)? (b) : (a))

MODULE_LICENSE("Dual BSD/GPL");

struct file_operations proc_fops = {
    .read = scull_read_procmem,
};

/*
 * As for the new version of kernel, as we don't have eof as one of the parameter.
 * We have to signal an EOF to any reading program, one of the simple tactics is:
 * using offp as an indication, *offp == 0, perform read; otherwise, return 0,
 * which signals EOF to reading programs such as cat;
 */

ssize_t scull_read_procmem(struct file *filp, char __user *buffer, size_t count, loff_t *offp)
{
    int i, j, k, len = 0;
    struct scull_dev *sdev;
    struct scull_qset *qset;
    void **data;
    char buf[BUFSIZE];
    const int limit = min(count, BUFSIZE) - 80;

    // the second read would return 0
    if(*offp > 0)
        return 0;

    ALOGD("scull_read_procmem: count = %ld\n", count);
    for(i = 0; i < gDev_nums && len <= limit; i++) {
        sdev = &gSdev[i];
        if(down_interruptible(&sdev->proc_sem))
            return -ERESTARTSYS;
        len += sprintf(buf+len, "Scull device-%i: qset-%i, quantum-%i, total size-%li\n", \
                i, sdev->qset, sdev->quantum, sdev->size);
        for(qset = sdev->data, j = 0; qset && len <= limit; qset = qset->next, j++) {
            len += sprintf(buf+len, "\tquantum set-%d at %8p, data at %8p\n", \
                    j, qset, qset->data);
            data = qset->data;
            // struct qset::data is NULL terminated
            for(k = 0; data && *data && len <= limit; ++data, ++k)
                len += sprintf(buf+len, "\t\tNO.%d data: %8p\n", \
                        k, *data);
        }

        up(&sdev->proc_sem);
    }
    copy_to_user(buffer, buf, len);
    *offp += len;

    return len;
}
