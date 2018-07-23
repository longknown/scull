#include <linux/cdev.h>
#include <linux/fs.h>

#define DEVICE_NAME "scull"
#define MODULE_NAME "scull"
#define SCULL_MAJOR 0
#define DEVICE_NUM  4
#define SCULL_SET   1024
#define SCULL_QUANTUM   4096

struct scull_qset {
    void **data;            /* end with NULL pointer*/
    struct scull_qset *next;
};

struct scull_dev {
    struct scull_qset *qset;    /* pointer to quantum set */
    int quantum;                /* the current quantum size */
    int qset;                   /* the current array size */
    unsigned long size;         /* amount of data stored here */
    unsigned int access_key;    /* used by sculluid and scullpriv */
    struct semaphore sem;       /* for mutex */
    struct cdev cdev;           /* char device struct */
};

loff_t (scull_llseek) (struct file *, loff_t, int);
ssize_t (scull_read) (struct file *, char __user *, size_t, loff_t *);
ssize_t (scull_write) (struct file *, const char __user *, size_t, loff_t *);
int (scull_open) (struct inode *, struct file *);
int (scull_release) (struct inode *, struct file *);
long (scull_ioctl) (struct file *, unsigned int, unsigned long);
int (scull_trim) (struct scull_dev *);
