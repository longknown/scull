#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/semaphore.h>
#include <linux/wait.h>

struct scull_pipe {
    wait_queue_head_t   inq, outq;              // wait queue for read and write processes
    char                *buf_begin, *buf_end;   // ptr to begin and end of buffer
    int                 bufsize;
    char                *rp, *wp;
    int                 nreads, nwrites;        // numbers of reads and writes
    struct fsync_struct *async_queue;           // asynchronous reads
    struct semaphore    sem;
    struct cdev         cdev;
};
