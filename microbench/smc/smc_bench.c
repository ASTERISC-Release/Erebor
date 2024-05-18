#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <sva/stack.h>
#include <sva/mmu.h>
#include <sva/enc.h>

#include "utils.h"

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
    return 0;
}

void __exit moduleExit(void) {
    printk("KMOD EXIT");
}

MODULE_LICENSE("GPL");

module_init(moduleInit);
module_exit(moduleExit);