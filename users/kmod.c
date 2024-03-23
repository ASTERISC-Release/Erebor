#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <sva/stack.h>
#include <sva/mmu.h>

// static struct kprobe kp = {
//     .symbol_name = "kallsyms_lookup_name"
// };

// unsigned long read_cr4(void) {
//     unsigned long cr4;
//     asm volatile ("mov %%cr4, %0" : "=r" (cr4));
//     return cr4;
// }

// void write_cr4(unsigned long cr4) {
//     asm volatile ("mov %0, %%cr4" : "=r" (cr4));
// }

// int pks_test_val = 10;

// int __init moduleInit(void) {
//     printk("MODULE INIT");

//     // unsigned long cr4 = read_cr4();
//     // printk("CR4 = %lx", cr4);
//     // cr4 |= (1ul << 24);
//     // write_cr4(cr4);

//     // cr4 = read_cr4();
//     // printk("CR4 = %lx", cr4);

//     typedef unsigned long (*kallsyms_lookup_name_t)(const char *name);
//     kallsyms_lookup_name_t kallsyms_lookup_name;
//     register_kprobe(&kp);
//     kallsyms_lookup_name = (kallsyms_lookup_name_t) kp.addr;
//     unregister_kprobe(&kp);

//     uintptr_t bool_addr = kallsyms_lookup_name("mmu_bool");
//     *(bool*)bool_addr = true;
//     printk("mmu_bool = %d\n", *(bool*)bool_addr);

//     // typedef uintptr_t page_entry_t;

//     // page_entry_t *(*get_pgeVaddr_ptr)(uintptr_t vaddr);
//     // get_pgeVaddr_ptr = (page_entry_t *(*)(uintptr_t))kallsyms_lookup_name("get_pgeVaddr");

//     // printk("Calling pks_test()");
//     // page_entry_t *entry = get_pgeVaddr_ptr((uintptr_t)&pks_test_val);

//     // printk("pks_test_val_addr1 = %lx", (uintptr_t)&pks_test_val);
//     // printk("pks_test_val_addr2 = %lx", (uintptr_t)entry);
//     // printk("pks_test_val_addr3 = %lx", *entry);
//     // // printk("pks_test_val_entry = %lx", (uintptr_t)__va((*entry)) | offset);
//     // uintptr_t base = (uintptr_t) *entry & 0x000ffffffffff000ul;
//     // uintptr_t offset = (uintptr_t)&pks_test_val & 0x0000000000000ffful;
//     // unsigned char* ans = __va((uintptr_t)base|offset); 
//     // printk("Value = %d", *(int*)ans);
    
//     // // printk("val_addr = %p, *val_addr = %d", (void*)val_addr, *(int*)val_addr);
    
//     return 0;
// }

// char SecureStack[1<<12];
// // TODO: Important this value can't be changed from outside the nested kernel!
// const uintptr_t SecureStackBase = (uintptr_t) SecureStack + sizeof(SecureStack);

uintptr_t get_stack_pointer(void) {
    uintptr_t sp;
    asm volatile (
        "movq %%rsp, %0\n\t"
        : "=r" (sp)
        :
        : "memory"
    );
    return sp;
}

SECURE_WRAPPER(void, hello, void) {
    printk("Hello from Secure Function");
    printk("SP = %lx", get_stack_pointer());
}

int __init moduleInit(void) {
    printk("MODULE INIT");

    printk("SP = %lx", get_stack_pointer());
    hello();
    printk("SP = %lx", get_stack_pointer());
    
    return 0;
}

void __exit moduleExit(void) {
    printk("KMOD EXIT");
}

MODULE_LICENSE("GPL");

module_init(moduleInit);
module_exit(moduleExit);