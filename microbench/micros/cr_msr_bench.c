#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <asm/msr.h>
#ifdef CONFIG_ENCOS
#include <sva/stack.h>
#include <sva/mmu.h>
#include <sva/msr.h>
#include <sva/enc.h>
#endif

#include "utils.h"

#ifdef CONFIG_ENCOS
#define MSG "Mearsuring ENCOS guest CR/WRMSR read/write overhead."
#else
#define MSG "Mearsuring NATIVE guest CR/WRMSR read/write overhead."
#endif

#define MSR_REG_EFER    0xC0000080      /* MSR for EFER register */

int __init moduleInit(void) {
    int i;
    uint64_t tsc, avg;
    
    printk(KERN_INFO "%s.\n", MSG);

    /* CR0 */
    avg = 0;
    for (i = 0; i < N_TIMES; i++) {
        tsc = rdtscp();
#ifdef CONFIG_ENCOS
        sva_write_cr0(native_read_cr0());
#else
        native_write_cr0(native_read_cr0());
#endif
        avg += (rdtscp() - tsc);
    }
    avg /= N_TIMES;
    printk("Write CR0 avg cycle: %llu\n", avg);


    /* WRMSR EFER */
    avg = 0;
    for (i = 0; i < N_TIMES; i++) {
        tsc = rdtscp();
#ifdef CONFIG_ENCOS
        bench_encos_write_msrl(MSR_REG_EFER, rdmsrl(MSR_REG_EFER));
#else
        wrmsrl(MSR_REG_EFER, rdmsrl(MSR_REG_EFER));
#endif
        avg += (rdtscp() - tsc);
    }
    avg /= N_TIMES;
    printk("Write MSR EFER avg cycle: %llu\n", avg);
    return 0;
}

void __exit moduleExit(void) {
    printk("KMOD EXIT");
}

MODULE_LICENSE("GPL");

module_init(moduleInit);
module_exit(moduleExit);