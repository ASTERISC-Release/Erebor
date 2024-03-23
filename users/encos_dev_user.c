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
    
    // mmap from device to the vma
    buf = mmap(NULL, 0x1000, PROT_READ|PROT_WRITE, MAP_PRIVATE, fd, 0);
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