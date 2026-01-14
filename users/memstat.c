#define _GNU_SOURCE
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <unistd.h> 
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>

#define   ENCOS_MEM_STAT              _IOW('m', 12, unsigned int)


#define ENCOS_DEV "/dev/encos-dev"

int main(int argc, char* argv[]) {
    /* open dev file */
    int fd = open(ENCOS_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open the device");
        return -1;
    }

    ioctl(fd, ENCOS_MEM_STAT, 0);

    return 0;
}