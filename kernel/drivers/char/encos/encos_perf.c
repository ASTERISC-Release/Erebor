#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <asm/msr.h>
#include <asm/pgtable_64.h>
#include <asm/pgtable_types.h>

#ifdef CONFIG_ENCOS
#include <sva/stack.h>
#include <sva/mmu.h>
#include <sva/enc.h>
#endif

#include <sva/idt.h>

#include "encos_perf.h"

#ifdef CONFIG_ENCOS
#define MSG "[ENCOS_MICRO_PERF] Mearsuring ENCOS guest CR/WRMSR read/write overhead."
#else
#define MSG "[ENCOS_MICRO_PERF] Mearsuring NATIVE guest CR/WRMSR read/write overhead."
#endif


void encos_micro_perf(void)
{
    int i;
    uint64_t tsc, avg;

    printk(KERN_INFO "%s.\n", MSG);

    /* SMC overhead */
#ifdef CONFIG_ENCOS
    avg = 0;
    for (i = 0; i < N_TIMES; i++) {
        tsc = rdtscp();
        SM_encos_empty();
        avg += (rdtscp() - tsc);
    }
    avg /= N_TIMES;
    printk("[ENCOS_MICRO_PERF] SMC EMPTY_CALL avg cycle: %llu\n", avg);
#endif

    /* write CR overhead */
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
    printk("[ENCOS_MICRO_PERF] Write CR0 avg cycle: %llu\n", avg);

    /* write MSR overhead */
    avg = 0;
    for (i = 0; i < N_TIMES; i++) {
        tsc = rdtscp();
#ifdef CONFIG_ENCOS
        bench_encos_write_msrl(MSR_REG_EFER, native_read_msr(MSR_REG_EFER));
#else
        wrmsrl(MSR_REG_EFER, native_read_msr(MSR_REG_EFER));
#endif
        avg += (rdtscp() - tsc);
    }
    avg /= N_TIMES;
    printk("[ENCOS_MICRO_PERF] Write MSR EFER avg cycle: %llu\n", avg);

    /* MMU update */
    avg = 0;
    void *ptep = (void*)__get_free_page(GFP_KERNEL | __GFP_ZERO);
    for (i = 0; i < N_TIMES; i++) {
        tsc = rdtscp();
#ifdef CONFIG_ENCOS
        sva_declare_l1_page(__pa(ptep));
#endif
        native_set_pte(ptep, native_make_pte(0));
#ifdef CONFIG_ENCOS
        sva_remove_page(__pa(ptep));
#endif
        avg += (rdtscp() - tsc);
    }
    avg /= N_TIMES;
    printk("[ENCOS_MICRO_PERF] MMU declate+update+remove avg cycle: %llu\n", avg);

    /* LIDT update */
    avg = 0;
    for (i = 0; i < N_TIMES; i++) {
        tsc = rdtscp();

        load_current_idt();

        avg += (rdtscp() - tsc);
    }
    avg /= N_TIMES;
    printk("[ENCOS_MICRO_PERF] LIDT avg cycle: %llu\n", avg);

    return;
}