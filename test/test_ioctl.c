#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include "../scull.h"

int main(int argc, char **argv)
{
    char *driver;
    int fd, cmd;
    int param;

    if(argc < 3) {
        printf("./a.out [DRIVER_NAME] [CMDs]:\n"
                "CMDs:\n"
                "\tRESET 0: to reset quantum and qset\n"
                "\tSQUANTUM 1, SQSET 2: set thru pointer\n"
                "\tTQUANTUM 3, TQSET 4: set thru argument value\n"
                "\tXQUANTUM 9, XQSET 10: exchange thru pointer\n"
                "\tHQUANTUM 11, HQSET 12: switch thru argument and return value\n");
        return -1;
    }

    driver = argv[1];
    if((fd = open(driver, O_RDONLY)) < 0) {
        dprintf(stderr, "invalid driver name provided: %s\n", driver);
        exit(1);
    }

    switch ((cmd=atoi(argv[2]))) {
        case RESET:
            if(ioctl(fd, cmd) < 0) {
                dprintf(stderr, "ioctl failed: RESET\n");
                exit(1);
            } else {
                printf("ioctl on RESET\n");
            }

            if(ioctl(fd, SYNCPARAMS) < ) {
                dprintf(stderr, "reset driver failed\n");
                exit(1);
            } else {
                printf("driver params reset!\n");
            }
            break;
        case SQUANTUM:
            if(argc != 4) {
                dprintf(stderr, "another param should be provided!\n");
                exit(1);
            }
            if((param = atoi(argv[3])) <= 0) {
                dprintf(stderr, "quantum param need to be positive\n");
                exit(1);
            }
            if(ioctl(fd, cmd, (void *)&param) < 0) {
            }
        case SQSET:
        case TQUANTUM:
        case TQSET:
        case XQUANTUM:
        case XQSET:
        case HQUANTUM:
        case HQSET:
            break;
        default:
            break;
    }
    return 0;
}
