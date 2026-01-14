#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <sva/stack.h>
#include <sva/mmu.h>

#include <asm/special_insns.h>

#include <linux/smp.h>


static void write_cr4(unsigned long bv) {
    unsigned long __cr4;
    /* 
     * simply enable PKS feature, without setting any
     * protection key permissions. (PKRS)
     */
    __cr4 = native_read_cr4() | (1 << 24);
    printk("cr4: prepare to write 0x%lx\n", __cr4);
    __asm__ volatile ("mov %0, %%cr4" : : "r" (__cr4));
    printk("cr4: finish to write 0x%lx\n", __cr4);
}


int __init moduleInit(void) {
    // write pks bit
    write_cr4(1 << 24);
    return 0;
}

void __exit moduleExit(void) {
    return;
}

MODULE_LICENSE("GPL");

module_init(moduleInit);
module_exit(moduleExit);