#include <asm/msr.h>
#include <sva/mmu.h>
#include <sva/pks.h>
#include <sva/stack.h>
#include <sva/mmu_intrinsics.h>
#include <linux/slab.h>
#include <asm/pgtable.h>
#include <asm/set_memory.h>

/*
 * For the PKS version, we always use key=1 for the protected page.
 * key=0 is reserved for the kernel (unprotected pages).
 */
int set_page_protection(uintptr_t virtual_page, int key)
{   
#ifdef CONFIG_ENCOS_PKS
    page_entry_t *page_entry;
    int level = 0;
    page_entry = get_pgeVaddr(virtual_page, &level);

    // early exit if virtual_page's PTE(key) == 0 && key (to be set) == 0; to avoid unnecessary page splits
    if(((*page_entry >> 59) & 0xf) == 0 && key == 0)
        return 0;
    
    if (level == 0) {
        //debug
        for (int i = 0; i < NCPU; i++){
            if (virtual_page == SyscallSecureStackBase + i * pageSize) {
                printk("NOT PROTECT PROTECT_PAGE of SyscallSecureStack: 0x%lx.\n", virtual_page);
            }
        }
        return -1;
    }

    /* if level = L2 (HugePage), then split the hugepage and set pks for the virtual_page's L1 mapping */
    /* Tightly copupling the split page function with pks.c for now */
    if(level == 2) {
        /* allocate a 4KB page */
        /* ENCOS (TODO) Fails for update_l1 mappings, very early on  during system boot;
           Replace with static PT pages for such cases ? */
        void* page = (void *)__get_free_pages(GFP_ATOMIC, 0);
        if (!page) {
            printk(KERN_ERR "Failed to allocate memory\n");
            return -ENOMEM;
        }

        /* Update the metadata for the L2 and L1 PTPs */
        /* Cannot call a sva_declare_l2_page_secure here, since it would attempt to re-acquire a the MMULock 
           hence, updating the page_desc manually */
        page_desc_t *pgDescL1 = getPageDescPtr(__pa(page));
        memset(pgDescL1, 0, sizeof(struct page_desc_t));
        pgDescL1->type = PG_L1;
        /* ENCOS (TODO) */
        // pgDescL1->pgVaddr = __pa((unsigned long)page_entry & 0xFFFFFFFFFFFFF000);
        // pgDescL1->count = 1;

        page_desc_t *pgDescL2 = getPageDescPtr(__pa(((unsigned long)page_entry & 0xFFFFFFFFFFFFF000)));
        pgDescL2->type = PG_L2;

        /* memset it to 0 */
        memset(page, 0, PAGE_SIZE);

        /* get the correct PTE flags for the L1 mappings */
        pgprot_t ref_prot;
		ref_prot = pmd_pgprot(*(pmd_t *)page_entry);
		ref_prot = pgprot_large_2_4k(ref_prot);
        ref_prot = pgprot_clear_protnone_bits(ref_prot);

        /* add mappings from the L1 page to the 512x(4KB) pages */
        for(int i = 0; i < 512; i++) {
            uintptr_t pte = pfn_pte(((*page_entry + i*PAGE_SIZE) >> 12), ref_prot).pte;
            *(uintptr_t*)(page+i*8) = pte;

            if(((virtual_page >> (12-3)) & 0xfff) == i) {
                /* add the key in the L1 mapping that maps the virtual page */
                *(uintptr_t*)(page+i*8) |= ((unsigned long long)key << 59);
            }
        }   

        /* Point the L2 page to the newly created L1 page */
        unsigned long page_nr = (unsigned long)__pa(page) >> 12;
        struct pgprot _pgprot = __pgprot(_KERNPG_TABLE);
        *page_entry = pfn_pte(page_nr, _pgprot).pte;

        /* ENCOS (TODO) Do we need to flush the entire virtual address range that maps to this new L2->L1 mapping (512 vaddr's) 
        or just the one vaddr which was protected ? */
        // flush_tlb_all();
    } else if(level == 1) {
        if(key == 0) {
            // set the existing key in the PTE to 0 so that we can bitwise OR the new key
            unsigned long mask = 0x87FFFFFFFFFFFFFF;
            *page_entry &= mask;
        }
        /* set protection key in PTE */
        *page_entry |= ((unsigned long long)key << 59);
    }
#endif

#ifdef CONFIG_ENCOS_WP
    page_entry_t *page_entry;
    int level;
    page_entry = get_pgeVaddr(virtual_page, &level);
    if (!level) {
        return;
    }
    /* set RW permission to 0 in PTE */
    if (key)
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
    return 0;
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