#include <asm/msr.h>
#include <sva/mmu.h>
#include <sva/pks.h>
#include <sva/stack.h>
#include <sva/mmu_intrinsics.h>

/*
 * For the PKS version, we always use key=1 for the protected page.
 * key=0 is reserved for the kernel (unprotected pages).
 */
void set_page_protection(uintptr_t virtual_page, int should_protect)
{   
#ifdef CONFIG_ENCOS_PKS
    page_entry_t *page_entry;
    unsigned long long key;
    int is_l1 = 0;
    page_entry = get_pgeVaddr(virtual_page, &is_l1);
    
    if (!is_l1) {
        //debug
        for (int i = 0; i < NCPU; i++){
            if (virtual_page == SyscallSecureStackBase + i * pageSize) {
                printk("NOT PROTECT PROTECT_PAGE of SyscallSecureStack: 0x%lx.\n", virtual_page);
            }
        }
        return;
    }
    /* set protection key in PTE */
    key = should_protect ? 1 : 0;
    *page_entry |= ((unsigned long long)key << 59);
#endif

#ifdef CONFIG_ENCOS_WP
    page_entry = get_pgeVaddr(virtual_page, &is_l1);
    if (!is_l1) {
        return;
    }
    /* set RW permission to 0 in PTE */
    if (should_protect)
        *page_entry &= ~PG_RW;
    else
        *page_entry |= PG_RW;
#endif
    //debug
    for (int i = 0; i < NCPU; i++){
        if (virtual_page == SyscallSecureStackBase + i * pageSize) {
            printk("PROTECT_PAGE of SyscallSecureStack: 0x%lx.\n", virtual_page);
        }
    }

    /* flush the TLB after the function */
    sva_mm_flush_tlb((void *)virtual_page);
}

/* Chuqi: useless for now. */
void pks_set_key(int key, bool restrictAccess, bool restrictWrite) {
    uint64_t pkrs;
    rdmsrl(0x6e1, pkrs);
    uint64_t access = 1ull << (key*2);
    uint64_t write = 1ull << (key*2 + 1);

    if(restrictAccess)
        pkrs |= access;
    else
        pkrs &= ~access;

    if(restrictWrite)
        pkrs |= write;
    else
        pkrs &= ~write;

    wrmsrl(0x6e1, pkrs);
}