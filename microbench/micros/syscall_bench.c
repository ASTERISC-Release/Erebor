#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include "utils.h"

#define __NR_SYS_ni     999
#define __NR_VMCALL_ni  9999
#define __NR_SYS_getpid 0x27
#define __NR_SYS_write  0x1


static inline uint64_t rdtsc_start(void) {
    unsigned int hi, lo;
    asm volatile ("cpuid\n\t"
                  "rdtsc\n\t"
                  "mov %%edx, %0\n\t"
                  "mov %%eax, %1\n\t"
                  : "=r" (hi), "=r" (lo)
                  :: "%rax", "%rbx", "%rcx", "%rdx");
    return ((uint64_t)hi << 32) | lo;
}

static inline uint64_t rdtsc_end(void) {
    unsigned int hi, lo;
    asm volatile ("rdtscp\n\t"
                  "mov %%edx, %0\n\t"
                  "mov %%eax, %1\n\t"
                  "cpuid\n\t"
                  : "=r" (hi), "=r" (lo)
                  :: "%rax", "%rbx", "%rcx", "%rdx");
    return ((uint64_t)hi << 32) | lo;
}

int main(void)
{
    int i, ret;
    uint64_t tsc, avg;
    const char *msg = "Hello, world!\n";

    /* syscall overhead */
    avg = 0;
    for (i = 0; i < N_TIMES; i++) {
        tsc = rdtsc_start();
        ret = (int)syscall_3(__NR_SYS_ni, 0, 0, 0);
        avg += (rdtsc_end() - tsc);
    }

    avg /= N_TIMES;
    printf("SYSCALL SYS_NI avg cycle=%lu, syscall ret=%d\n", 
            avg, ret);

    /* hypercall overhead */
    avg = 0;
    for (i = 0; i < N_TIMES; i++) {
        tsc = rdtsc_start();
        ret = (int)hypercall_3(__NR_VMCALL_ni, 0, 0, 0);
        avg += (rdtsc_end() - tsc);
    }
    avg /= N_TIMES;
    printf("HYPERCALL VMCALL_NI ret=%d avg cycle=%llu\n", ret, avg);
    return 0;
}
