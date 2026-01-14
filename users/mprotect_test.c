#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#define PG_SIZE 0x1000

int main() {
    /* get a normal page */
    void* buffer = mmap(NULL, PG_SIZE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (buffer == MAP_FAILED) {
        perror("mmap");
        return 1;
    }
    printf("mmap buf: 0x%lx -> PROT_READ | PROT_WRITE success\n", (unsigned long)buffer);

    /* get a prot_none page? */
    void *buffer2 = mmap(NULL, PG_SIZE, PROT_NONE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (buffer2 == MAP_FAILED) {
        perror("mmap");
        return 1;
    }
    printf("mmap buf2: 0x%lx -> PROT_NONE success\n", (unsigned long)buffer2);

    /* write data */
    char* data = buffer;
    data[0] = 'A';

    /* mprotect the region to read-only */
    printf("Start mprotect -> PROT_READ\n");
    if (mprotect(buffer, PG_SIZE, PROT_READ) == -1) {
        perror("mprotect");
        munmap(buffer, PG_SIZE);
        return 1;
    } else {
        printf("mprotect -> PROT_READ success\n");
    }

    /* mprotect the region to PROT_NONE */
    printf("Start mprotect -> PROT_NONE\n");
    if (mprotect(buffer, PG_SIZE, PROT_NONE) == -1) {
        perror("mprotect");
        munmap(buffer, PG_SIZE);
        return 1;
    } else {
        printf("mprotect -> PROT_NONE success\n");
    }

    /* finish */
    munmap(buffer, PG_SIZE);
    return 0;
}
