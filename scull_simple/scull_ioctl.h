#include <linux/ioctl.h>
/*
 * S => Set, thru a pointer
 * T => Tell, thru argument value
 * G => Get, reply thru a pointer
 * Q => Query, reply with a return value
 * X => eXchange, switch G&S automatically
 * H => sHift, switch Q&T automatically
 */
enum {
    RESET   = 0,
    SQUANTUM,
    SQSET,
    TQUANTUM,
    TQSET,
    GQUANTUM,
    GQSET,
    QQUANTUM,
    QQSET,
    XQUANTUM,
    XQSET,
    HQUANTUM,
    HQSET,
    MAXNR   = 13,
};

/*
 * IOCTL defines for scull driver
 */
#define SCULL_IOC_MAGIC     'k'

#define SCULL_IOCRESET      _IO(SCULL_IOC_MAGIC, RESET)
#define SCULL_IOCSQUANTUM   _IOW(SCULL_IOC_MAGIC, SQUANTUM, int)
#define SCULL_IOCSQSET      _IOW(SCULL_IOC_MAGIC, SQSET, int)
#define SCULL_IOCTQUANTUM   _IO(SCULL_IOC_MAGIC, TQUANTUM)
#define SCULL_IOCTQSET      _IO(SCULL_IOC_MAGIC, TQSET)
#define SCULL_IOCGQUANTUM   _IOR(SCULL_IOC_MAGIC, GQUANTUM, int)
#define SCULL_IOCGQSET      _IOR(SCULL_IOC_MAGIC, GQSET, int)
#define SCULL_IOCQQUANTUM   _IO(SCULL_IOC_MAGIC, QQUANTUM)
#define SCULL_IOCQQSET      _IO(SCULL_IOC_MAGIC, QQSET)
#define SCULL_IOCXQUANTUM   _IOWR(SCULL_IOC_MAGIC, XQUANTUM, int)
#define SCULL_IOCXQSET      _IOWR(SCULL_IOC_MAGIC, XQSET, int)
#define SCULL_IOCHQUANTUM   _IO(SCULL_IOC_MAGIC, HQUANTUM)
#define SCULL_IOCHQSET      _IO(SCULL_IOC_MAGIC, HQSET)

#ifndef __KERNEL__
// for userspace cmd mapping
# define CMD(name) {name, SCULL_IOC ## name}
typedef struct {int cmd; int cmd_ioctl;} ioctl_t;

ioctl_t scull_ioctl[] = {
    CMD(RESET),
    CMD(SQUANTUM),
    CMD(SQSET),
    CMD(TQUANTUM),
    CMD(TQSET),
    CMD(GQUANTUM),
    CMD(GQSET),
    CMD(QQUANTUM),
    CMD(QQSET),
    CMD(XQUANTUM),
    CMD(XQSET),
    CMD(HQUANTUM),
    CMD(HQSET),
};
#endif
