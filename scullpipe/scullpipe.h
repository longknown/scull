#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/wait.h>

#define BUFSIZE     (1 << 12)
#define DEV_NAME    "scull_pipe"

//#define NDEBUG

// patch "\n" at the end
#ifdef NDEBUG
# ifdef __KERNEL__
#  define ALOGV(fmt, ...) \
        ((void) printk(KERN_INFO fmt "\n", ## __VA_ARGS__))
# else
#  define ALOGV(fmt, ...) \
        ((void) fprintf(stderr, fmt "\n", ## __VA_ARGS__))
# endif
#else
# define ALOGV(...)  \
        ((void) 0)
#endif


#ifdef __KERNEL__
# define ALOGD(fmt, ...) \
    ((void) printk(KERN_ALERT fmt "\n", ## __VA_ARGS__))
#else
# define ALOGD(fmt, ...) \
    ((void) fprintf(stderr, fmt "\n", ## __VA_ARGS__))
#endif

struct scullp_cdev {
    wait_queue_head_t   inq, outq;              // wait queue for read and write processes
    char                *buf_begin, *buf_end;   // ptr to begin and end+1 of the buffer
    int                 bufsize;
    char                *rp, *wp;
    int                 nreads, nwrites;        // numbers of reads and writes
    struct fsync_struct *async_queue;           // asynchronous reads
    struct semaphore    sem;
    struct cdev         cdev;
};

loff_t scullp_llseek(struct file*, loff_t, int);
ssize_t scullp_read(struct file*, char __user *, size_t, loff_t *);
ssize_t scullp_write(struct file*, const char __user *, size_t, loff_t *);
int scullp_open(struct inode*, struct file*);
int scullp_ioctl(struct inode*, struct file*, unsigned int, unsigned long);
int scull_release(struct inode*, struct file*);
