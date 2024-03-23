#include <fcntl.h> // open
#include <sys/mman.h> // mmap
#include <unistd.h> 
#include <stdio.h>

#define ENCOS_DEV "/dev/encos-dev"

char *buf;

int main() {
    char buffer[128];
    int fd = open(ENCOS_DEV, O_RDONLY);
    if (fd < 0) {
        perror("Failed to open the device");
        return -1;
    }

    // what if i open it 3 times?
    int fd2 = open(ENCOS_DEV, O_RDONLY);
    if (fd2 < 0) {
        perror("Failed to open the device");
        return -1;
    }

    int fd3 = open(ENCOS_DEV, O_RDONLY);
    if (fd3 < 0) {
        perror("Failed to open the device");
        return -1;
    }

    // print all fds
    printf("fd: %d, fd2: %d, fd3: %d\n", fd, fd2, fd3);
    
    // determine a random va
    unsigned long va = 0x100000000;

    // mmap from device to the vma
    buf = mmap(va, 0x1000, PROT_READ|PROT_WRITE, MAP_PRIVATE, fd, 0);
    if (buf == MAP_FAILED) {
        perror("Failed to mmap the device");
        return -1;
    }
    printf("Mapped buffer: %p\n", buf);
    
    // unmap
    munmap(buf, 0x1000);

    close(fd);
    return 0;
}