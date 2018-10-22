#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/semaphore.h>
#include "scull_ioctl.h"

// uncomment NDEBUG to enable ALOGV
//#define NDEBUG

#define DEVICE_NAME "scull"
#define MODULE_NAME "scull"
#define SCULL_MAJOR 0
#define DEVICE_NUM  4
#define SCULL_SET   1024
#define SCULL_QUANTUM   4096

#ifdef NDEBUG
# ifdef __KERNEL__
#  define ALOGV(fmt, ...) \
        ((void) printk(KERN_INFO fmt, ## __VA_ARGS__))
# else
#  define ALOGV(fmt, ...) \
        ((void) fprintf(stderr, fmt, ## __VA_ARGS__))
# endif
#else
# define ALOGV(...)  \
        ((void) 0)
#endif


#ifdef __KERNEL__
# define ALOGD(fmt, ...) \
    ((void) printk(KERN_ALERT fmt, ## __VA_ARGS__))
#else
# define ALOGD(fmt, ...) \
    ((void) fprintf(stderr, fmt, ## __VA_ARGS__))
#endif

struct scull_qset {
    void **data;            /* end with NULL pointer*/
    struct scull_qset *next;
};

struct scull_dev {
    struct scull_qset *data;    /* pointer to quantum set */
    int quantum;                /* the current quantum size */
    int qset;                   /* the current array size */
    unsigned long size;         /* amount of data stored here */
    unsigned int access_key;    /* used by sculluid and scullpriv */
    struct semaphore sem;       /* main mutex to lock file ops*/
    struct semaphore proc_sem;  /* mutex for /proc/ reading */
    struct cdev cdev;           /* char device struct */
};
extern struct scull_dev gSdev[];
extern int gScull_major, gScull_minor, gDev_nums;
extern int gScull_qset, gScull_quantum;

/*
 * file operations of scull
 */
loff_t (scull_llseek) (struct file *, loff_t, int);
ssize_t (scull_read) (struct file *, char __user *, size_t, loff_t *);
ssize_t (scull_write) (struct file *, const char __user *, size_t, loff_t *);
int (scull_open) (struct inode *, struct file *);
int (scull_release) (struct inode *, struct file *);
long (scull_ioctl) (struct file *, unsigned int, unsigned long);
int (scull_trim) (struct scull_dev *);

struct scull_qset *scull_follow(struct scull_dev *, int);
int scull_alloc(struct scull_dev *);

/*
 * For /proc file implementations
 */
extern struct file_operations proc_fops;

#ifdef USE_SEQ  // using seq_file implementation, the default method
    void * (scull_seq_start) (struct seq_file *m, loff_t *pos);
    void (scull_seq_stop) (struct seq_file *m, void *v);
    void * (scull_seq_next) (struct seq_file *m, void *v, loff_t *pos);
    int (scull_seq_show) (struct seq_file *m, void *v);
    int (scull_proc_open) (struct inode *, struct file *);
#else   // using traditional proc_read method
    ssize_t (scull_read_procmem) (struct file *, char __user *, size_t, loff_t *);
#endif

