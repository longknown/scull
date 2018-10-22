#include <linux/seq_file.h>
#include "scull.h"



static struct seq_operations seq_fops= {
    .start  = scull_seq_start,
    .show   = scull_seq_show,
    .next   = scull_seq_next,
    .stop   = scull_seq_stop
};

struct file_operations proc_fops = {
    .open       = scull_proc_open,
    .owner      = THIS_MODULE,
    .read       = seq_read,
    .llseek     = seq_lseek,
    .release    = seq_release
};


int (scull_proc_open) (struct inode *inode, struct file *filp)
{
    return seq_open(filp, &seq_fops);
}


void * (scull_seq_start) (struct seq_file *m, loff_t *pos)
{
    if(*pos >= gDev_nums)
        return NULL;
    return (void *) (gSdev + *pos);
}

void (scull_seq_stop) (struct seq_file *m, void *v)
{
    return;
}
void * (scull_seq_next) (struct seq_file *m, void *v, loff_t *pos)
{
    ++*pos;
    if(*pos >= gDev_nums)
        return NULL;
    return (void *) (gSdev + *pos);
}

int (scull_seq_show) (struct seq_file *m, void *v)
{
    int i, j;
    struct scull_dev *sdev = (struct scull_dev *)v;
    void **data;
    struct scull_qset *qset;


    if(down_interruptible(&sdev->proc_sem))
        return -ERESTARTSYS;
    seq_printf(m, "Scull device-%li: qset-%i, quantum-%i, total size-%li\n", \
            (sdev-gSdev), sdev->qset, sdev->quantum, sdev->size);
    for(qset = sdev->data, i = 0; qset; qset = qset->next, i++) {
        seq_printf(m, "\tquantum set-%d at %8p, data at %8p\n", \
                i, qset, qset->data);
        data = qset->data;
        // struct qset::data is NULL terminated
        for(j = 0; data && *data; ++data, ++j)
            seq_printf(m, "\t\tNO.%d data: %8p\n", \
                    j, *data);
    }

    up(&sdev->proc_sem);
    return 0;
}
