#include <stdio.h>
#include "../scull.h"



int main(int argc, char **argv)
{
    char *driver;

    if(argc < 3) {
        printf("./a.out [DRIVER_NAME] [CMDs]:\n"
                "CMDs:\n"
                "\tRESET: to reset quantum and qset\n"
                "\tSQUANTUM, SQSET: set thru pointer\n"
                "\tTQUANTUM, TQSET: set thru argument value\n"
                "\tXQUANTUM, XQSET: exchange thru pointer\n"
                "\tHQUANTUM, HQSET: switch thru argument and return value\n");
        return -1;
    }
    return 0;
}
