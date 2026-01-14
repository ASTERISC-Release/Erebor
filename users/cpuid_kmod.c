#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <sva/stack.h>
#include <sva/mmu.h>

#include <linux/smp.h>
/*
 * Chuqi:
 * Refer to there for different ways to get the CPU core id.
 * https://stackoverflow.com/questions/22310028/is-there-an-x86-instruction-to-tell-which-core-the-instruction-is-being-run-on
 */

void get_cpuid(void *dummy)
{
    /* first try rdtscp */
    uint32_t eax, edx, IA32_TSC_AUX, core_id;
    uint64_t tsc;
    __asm__ volatile ("rdtscp"
                      : "=a" (eax), "=d" (edx), "=c" (IA32_TSC_AUX));
    tsc = ((uint64_t)edx << 32) | eax;
    core_id = IA32_TSC_AUX & 0xFFF;

    printk(KERN_INFO "rdtscp: tsc = 0x%llx, core_id = 0x%x, TSC_AUX=0x%x\n", 
                        tsc, core_id, IA32_TSC_AUX);

    /* second try cpuid */
}


int __init moduleInit(void) {
    on_each_cpu(get_cpuid, NULL, 1);
    return 0;
}

void __exit moduleExit(void) {
    printk("KMOD EXIT");
}

MODULE_LICENSE("GPL");

module_init(moduleInit);
module_exit(moduleExit);