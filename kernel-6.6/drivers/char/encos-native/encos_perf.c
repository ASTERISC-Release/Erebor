#include <linux/mm.h>
#include <linux/cma.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/sched.h>
#include <linux/debugfs.h>
#include <linux/io.h>
#include <linux/moduleparam.h>
#include <linux/delay.h>
#include <linux/highmem.h>
#include <asm/pgtable.h>
#include <linux/kallsyms.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kprobes.h>
#include <asm/msr.h>
#include <asm/pgtable_64.h>
#include <asm/pgtable_types.h>

#include <asm/msr-index.h>


#include "encos_perf.h"

#define MSG "[ENCOS_MICRO_PERF] Mearsuring NATIVE guest overhead."

struct miscdevice *misc;
struct mutex encos_dev_mlock;

static struct file_operations encos_dev_ops = {
    .owner = THIS_MODULE,
};

// static void
// ____wrmsr(unsigned long msr, uint64_t newval)
// {
// 	uint32_t low, high;

// 	low = newval;
// 	high = newval >> 32;
// 	__asm __volatile("wrmsr" : : "a" (low), "d" (high), "c" (msr));
// }

static uint64_t
____rdmsr(unsigned long msr)
{
	uint32_t low, high;

	__asm __volatile("rdmsr" : "=a" (low), "=d" (high) : "c" (msr));
	return (low | ((uint64_t)high << 32));
}

void encos_micro_perf(void)
{
    int i;
    
    uint64_t tsc, e, avg;

    printk(KERN_INFO "%s.\n", MSG);


    /* rdtscp */
    avg = 0;
    for (i = 0; i < N_TIMES; i++) {
        tsc = rdtscp();
        rdtscp();
        e = rdtscp();
        if (e >= tsc) {
                printk("tsc=%llu, e=%llu, e-tsc = %llu\n", tsc, e, (e-tsc));
                avg += (e - tsc);
        }
    }
    avg /= N_TIMES;
    printk("[ENCOS_MICRO_PERF] rdtscp avg cycle: %llu\n", avg);

    /* MMU update */
    void *ptep = (void*)__get_free_page(GFP_KERNEL | __GFP_ZERO);
    
    avg = 0;
    for (i = 0; i < N_TIMES; i++) {
        tsc = rdtscp();
        *(pte_t *)ptep = native_make_pte(0);
        e = rdtscp();
        if (e >= tsc) {
                avg += (e - tsc);
                printk("tsc=%llu, e=%llu, e-tsc = %llu\n", tsc, e, (e-tsc));
        }
    }
    avg /= N_TIMES;
    printk("[ENCOS_MICRO_PERF] MMU declate+update+remove avg cycle: %llu\n", avg);

    /* write CR overhead */
    avg = 0;
    for (i = 0; i < N_TIMES; i++) {
        tsc = rdtscp();
        native_write_cr0(native_read_cr0());
        avg += (rdtscp() - tsc);
    }
    avg /= N_TIMES;
    printk("[ENCOS_MICRO_PERF] Write CR0 avg cycle: %llu\n", avg);

    /* write MSR overhead */
    avg = 0;
    for (i = 0; i < N_TIMES; i++) {
        tsc = rdtscp();

        wrmsrl(MSR_LSTAR, ____rdmsr(MSR_LSTAR));

        avg += (rdtscp() - tsc);
    }
    avg /= N_TIMES;
    printk("[ENCOS_MICRO_PERF] Read/Write MSR MSR_LSTAR avg cycle: %llu\n", avg);
    return;
}

static int __init encos_perf_init(void)
{
    int rvl;

    /* register device */
    misc = kzalloc(sizeof(struct miscdevice), GFP_KERNEL);
    misc->name = "USELESS";
    misc->minor = MISC_DYNAMIC_MINOR;
    /* 
     * Note: this mode (0666) is ONLY for development purpose.
     * Granting all users to access the device will increase
     * the risk of security.
     */
    misc->mode = 0666; // rw-rw-rw-
    misc->fops = &encos_dev_ops;

    rvl = misc_register(misc);
    if (rvl) {
        printk("Failed to register misc device: %s\n", "USELESS");
        return rvl;
    }

    /* perf_test */
    encos_micro_perf();
    return 0;
}

static void __exit encos_perf_exit(void)
{
    printk("Exit.\n");
     /* deregister device */
    misc_deregister(misc);
    kfree(misc);
}

module_init(encos_perf_init);
module_exit(encos_perf_exit);