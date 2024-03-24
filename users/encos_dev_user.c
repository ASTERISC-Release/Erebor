#include <fcntl.h> // open
#include <sys/mman.h> // mmap
#include <unistd.h> 
#include <stdio.h>

#define ENCOS_DEV "/dev/encos-dev"

char *buf;

int main() {
    char buffer[128];
    int fd = open(ENCOS_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open the device");
        return -1;
    }

    // determine a random va
    unsigned long va = 0x100000000;

    // mmap from device to the vma
#define SZ_1M 0x100000
// #define MAP_FLAGS MAP_PRIVATE
#define MAP_FLAGS MAP_SHARED

    buf = mmap(NULL, SZ_1M, PROT_READ|PROT_WRITE, MAP_FLAGS, fd, 0);
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