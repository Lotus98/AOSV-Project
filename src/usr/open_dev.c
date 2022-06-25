#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../lib/ums.h"

#define DEVICE_NAME "/dev/umsdev"

int main ()
{
        int fd, res;

        fd = open(DEVICE_NAME, O_RDWR);
        if (fd < 0) {
                perror("Open umsdev");
        }
        res = ioctl(fd, 0x1337, NULL);
        printf("Result of ioctl call: %d\n", res);
        close(fd);

        return 0;
}
