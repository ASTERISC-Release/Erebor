#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include "utils.h"

#define __NR_SYS_ni     999
#define __NR_SYS_getpid 0x27
#define __NR_SYS_write  0x1

int main(void)
{
    int i, ret;
    uint64_t tsc, avg;
    const char *msg = "Hello, world!\n";

    /* syscall overhead */
    avg = 0;
    for (i = 0; i < N_TIMES; i++) {
        tsc = rdtscp();
        // ret = (int)syscall_3(__NR_SYS_write, STDOUT_FILENO, (uint64_t)msg, 14);
        ret = (int)syscall_3(__NR_SYS_ni, 0, 0, 0);
        avg += (rdtscp() - tsc);
    }

    avg /= N_TIMES;
    printf("SYSCALL SYS_NI avg cycle=%lu, syscall ret=%d\n", 
            avg, ret);

    /* hypercall overhead */
    avg = 0;
    for (i = 0; i < N_TIMES; i++) {
        tsc = rdtscp();
        ret = (int)hypercall_3(__NR_VMCALL_ni, 0, 0, 0);
        avg += (rdtscp() - tsc);
    }
    avg /= N_TIMES;
    printf("HYPERCALL VMCALL_NI avg cycle: %llu\n", avg);
    return 0;
}