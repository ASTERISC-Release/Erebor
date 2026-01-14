#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <sva/stack.h>
#include <sva/mmu.h>
#include <sva/enc.h>

#include "utils.h"

#define __NR_SYS_ni     999 // use undefined syscall
#define __NR_VMCALL_ni  999 // use undefined hypercall

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
    return 0;
}

int __init moduleInit(void) {
    int i;
    uint64_t tsc, avg;
    /* SMC overhead */
    avg = 0;
    for (i = 0; i < N_TIMES; i++) {
        tsc = rdtscp();
        SM_encos_empty();
        avg += (rdtscp() - tsc);
    }
    avg /= N_TIMES;
    printk("SMC EMPTY_CALL avg cycle: %llu\n", avg);
    printf("Please use dmesg to print the SMC result.\n");
    
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
    return 0;
    
    /* hypercall overhead */
    avg = 0;
    for (i = 0; i < N_TIMES; i++) {
        tsc = rdtscp();
        ret = (int)hypercall_3(__NR_VMCALL_ni, 0, 0, 0);
        avg += (rdtscp() - tsc);
    }
    avg /= N_TIMES;
    printk("HYPERCALL VMCALL_NI avg cycle: %llu\n", avg);
}

void __exit moduleExit(void) {
    printk("KMOD EXIT");
}

MODULE_LICENSE("GPL");

module_init(moduleInit);
module_exit(moduleExit);