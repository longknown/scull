#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include "../scull_ioctl.h"

#define PARAM2CMD(cmd)  SCULL_IOC ## #cmd

int main(int argc, char **argv)
{
    char *driver;
    int fd, cmdnum, cmd;
    int param, err;

    if(argc < 3) {
        printf("./a.out [DRIVER_NAME] [CMDs]:\n"
                "CMDs:\n"
                "\tRESET 0: to reset quantum and qset\n"
                "\tSQUANTUM 1, SQSET 2: set thru pointer\n"
                "\tTQUANTUM 3, TQSET 4: set thru argument value\n");
        return -1;
    }

    driver = argv[1];
    if((fd = open(driver, O_RDONLY)) < 0) {
        fprintf(stderr, "invalid driver name provided: %s\n", driver);
        exit(1);
    }


    cmdnum = atoi(argv[2]);
    cmd = scull_ioctl[cmdnum].cmd_ioctl;
    switch (cmdnum) {
        case RESET:
            if(ioctl(fd, cmd) < 0) {
                fprintf(stderr, "ioctl failed: RESET\n");
                exit(1);
            } else {
                printf("ioctl on RESET\n");
            }

            break;
        case SQUANTUM:
        case SQSET:
        case TQUANTUM:
        case TQSET:
            if(argc != 4) {
                fprintf(stderr, "another param should be provided!\n");
                exit(1);
            }
            param = atoi(argv[3]);
            switch(cmdnum) {
                case SQUANTUM:
                case SQSET:
                    err = ioctl(fd, cmd, (void *)&param);
                    break;
                case TQUANTUM:
                case TQSET:
                    err = ioctl(fd, cmd, (unsigned long) param);
                    break;
            }

            if(err < 0) {
                fprintf(stderr, "ioctl failed\n");
                exit(1);
            } else {
                printf("ioctl succeeded!\n");
            }
            break;
        default:
            fprintf(stderr, "error: unsupported cmd(%d)\n", cmd);
            break;
    }
    return 0;
}
