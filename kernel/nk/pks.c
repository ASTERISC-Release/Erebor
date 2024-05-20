#include <asm/msr.h>
#include <sva/mmu.h>
#include <sva/pks.h>

// void pks_update_mapping(uintptr_t vaddr, int key) {
//     page_entry_t *page_entry = get_pgeVaddr(vaddr);
//     // printk(KERN_INFO "[pks_update_mapping] vaddr=0x%lx, page_entry=0x%lx\n",
//             //  (unsigned long)vaddr, (unsigned long)page_entry);
//     *page_entry |= ((unsigned long long)key << 59);
// }

/*
 * For the PKS version, we always use key=1 for the protected page.
 * key=0 is reserved for the kernel (unprotected pages).
 */
void set_page_protection(uintptr_t virtual_page, int should_protect)
{
    int is_l1 = 0;
    page_entry_t *page_entry = get_pgeVaddr(virtual_page, &is_l1);
    unsigned long long key;
#ifdef CONFIG_ENCOS_PKS
    if (!is_l1) {
        return;
    }
    /* set protection key in PTE */
    key = should_protect ? 1 : 0;
    *page_entry |= ((unsigned long long)key << 59);
#endif

#ifdef CONFIG_ENCOS_WP
    /* set RW permission in PTE */
    /* Chuqi: TODO */
    panic("TBD!");
#endif
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