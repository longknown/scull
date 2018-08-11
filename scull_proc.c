/*
 * source file dedicated to /proc/ file creating and implementation
 */
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include "scull.h"

MODULE_LICENSE("Dual BSD/GPL");

struct file_operations proc_fops = {
    .read = scull_read_procmem,
};

ssize_t scull_read_procmem(struct file *filp, char __user *buffer, size_t count, loff_t *offp)
{
    int i, j, k, len = 0;
    const int limit = count - 80;
    struct scull_dev *sdev;
    struct scull_qset *qset;
    void **data;

    char *buf = kmalloc(count, GFP_KERNEL);
    if(!buf) {
        ALOGD("scull_read_procmem: kmalloc failed\n");
        return 0;
    }

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

    return len;
}
