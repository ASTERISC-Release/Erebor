#include <fcntl.h> // open
#include <sys/mman.h> // mmap
#include <unistd.h> 
#include <stdio.h>

#define ENCOS_DEV "/dev/encos-dev"

char *buf;

int main() {
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

#define PROTS (PROT_READ | PROT_WRITE)

    printf("mmaping prots: 0x%x, flags: 0x%x\n", 
                PROTS, MAP_FLAGS);

    buf = mmap(NULL, SZ_1M, PROTS, MAP_FLAGS, fd, 0);
    if (buf == MAP_FAILED) {
        perror("Failed to mmap the device");
        return -1;
    }
    printf("Mapped buffer: %p\n", buf);
    
    // unmap
    munmap(buf, SZ_1M);

    close(fd);
    return 0;
}
