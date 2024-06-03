#include <asm/msr.h>
#include <sva/mmu.h>
#include <sva/svamem.h>
#include <sva/pks.h>
#include <sva/stack.h>
#include <sva/mmu_intrinsics.h>
#include <linux/slab.h>
#include <asm/pgtable.h>
#include <asm/set_memory.h>

// TODO DEBUG
static inline unsigned long read_cr3(void) {
    unsigned long cr3;
    asm volatile("mov %%cr3, %0" : "=r" (cr3));
    return cr3;
}

// in total 16MB
#define N_INTERNAL_PAGES    0x1000
// a page = 0x1000 bytes = 4KB; 1 bytes = 8 bits = void
static char internal_split_PTPs[N_INTERNAL_PAGES][0x1000] SVAMEM __attribute__((aligned(0x1000)));

static int avail_index = 0;

char *__internal_alloc_PTP(void)
{
    // if (mmuIsInitialized) {
    //     return (char *)__get_free_pages(GFP_ATOMIC, 0);   
    // }
    if (avail_index >= N_INTERNAL_PAGES) {
        panic("No free space.\n");
    }
    return (char *)internal_split_PTPs[avail_index++];
}

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

    printk("pks set_page_protection: START level=%d; entryVA=0x%lx, entry=0x%lx, type: %d, key = %d\n", 
        level, (unsigned long)page_entry, *(unsigned long*)page_entry, getPageDescPtr(__pa(virtual_page))->type, key);

    // early exit if virtual_page's PTE(key) == 0 && key (to be set) == 0; to avoid unnecessary page splits
    if(((*page_entry >> 59) & 0xf) == 0 && key == 0) {
        // printk("set page protection: skipping page (va=0x%lx, pa=0x%lx, entry=0x%lx, entryVA=0x%lx) with key=0\n", 
        //             virtual_page, __pa(virtual_page), *page_entry, (unsigned long)page_entry);
        return 0;
    }
        
    
    // no mapping found
    // if (level == 0) {
    //     //debug
    //     for (int i = 0; i < NCPU; i++){
    //         if (virtual_page == SyscallSecureStackBase + i * pageSize) {
    //             printk("NOT PROTECT PROTECT_PAGE of SyscallSecureStack: 0x%lx.\n", virtual_page);
    //         }
    //     }
    //     return -1;
    // }

    /* 
     * if level = L2 (HugePage), then split the hugepage and set pks for the virtual_page's L1 mapping 
     */
    /* Tightly copupling the split page function with pks.c for now */
    if(level == 2) {
        /* allocate a 4KB page */
        /* ENCOS (TODO) Fails for update_l1 mappings, very early on  during system boot;
           Replace with static reserved PT pages for such cases ? */
        // void* page = (void *)__get_free_pages(GFP_ATOMIC, 0);
        void *page = (void *)__internal_alloc_PTP();
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
            
            // TODO debug print here
            if(((virtual_page >> (12-3)) & 0xfff) == i) {
                printk("[before settings] set page protection: CR3=0x%lx, page (va=0x%lx, pa=0x%lx, entry=0x%lx, entryVA=0x%lx, type=%d) with key=%d (splitted idx=%d)\n", 
                    read_cr3(), virtual_page, __pa(virtual_page), *(unsigned long*)(page+i*8), (unsigned long)((page+i*8)), getPageDescPtr(__pa(virtual_page))->type, key, i);
            
                int _l;
                page_entry_t *_pte_ddd = get_pgeVaddr((unsigned long)(page+i*8), &_l);
                printk(" --> L1 PTE of the entry PTE=0x%lx, (PTE_VA=0x%lx) level=%d\n",
                                        *_pte_ddd, (unsigned long)_pte_ddd, _l);
            }

            
            *(uintptr_t*)(page+i*8) = pte;

            /* only mask the sensitive index */
            if(((virtual_page >> (12-3)) & 0xfff) == i) {
                /* add the key in the L1 mapping that maps the virtual page */
                *(uintptr_t*)(page+i*8) |= ((unsigned long long)key << 59);
                
                printk("[after setting] set page protection: CR3=0x%lx, page (va=0x%lx, pa=0x%lx, entry=0x%lx, entryVA=0x%lx, type=%d) with key=%d (splitted idx=%d)\n", 
                    read_cr3(), virtual_page, __pa(virtual_page), *(unsigned long*)(page+i*8), (unsigned long)((page+i*8)), getPageDescPtr(__pa(virtual_page))->type, key, i);
            
                int _l;
                page_entry_t *_pte_ddd = get_pgeVaddr((unsigned long)(page+i*8), &_l);
                printk(" --> L1 PTE of the entry PTE=0x%lx, (PTE_VA=0x%lx) level=%d\n",
                                        *_pte_ddd, (unsigned long)_pte_ddd, _l);
            }
        }

        /* Point the splitted L2 page entry to the newly created L1 page */
        unsigned long page_nr = (unsigned long)__pa(page) >> 12;
        struct pgprot _pgprot = __pgprot(_KERNPG_TABLE);
        *page_entry = pfn_pte(page_nr, _pgprot).pte;

        /* debug print */
        int index = (virtual_page >> (12-3)) & 0xfff;
        int _ll;
        page_entry_t *_pte_xxx = get_pgeVaddr((unsigned long)(page+index*8), &_ll);
        printk("[FINAL] CR3=0x%lx --> L1 PTE of the entry PTE=0x%lx, (PTE_VA=0x%lx) level=%d\n",
                                read_cr3(), *_pte_xxx, (unsigned long)_pte_xxx, _ll);

        /* ENCOS (TODO) Do we need to flush the entire virtual address range that maps to this new L2->L1 mapping (512 vaddr's) 
        or just the one vaddr which was protected ? */
        // flush_tlb_all();
    } else if(level == 1) {
        if(key == 0) {
            // set the existing key in the PTE to 0 so that we can bitwise OR the new key
            unsigned long mask = 0x87FFFFFFFFFFFFFF;
            *page_entry &= mask;
        }
        page_desc_t * cccc = getPageDescPtr(__pa(virtual_page));
        /* set protection key in PTE */
        *page_entry |= ((unsigned long long)key << 59);
        printk("set page protection: CR3=0x%lx, page (va=0x%lx, pa=0x%lx, entry=0x%lx, entryVA=0x%lx, type=%d) with key=%d\n", 
                    read_cr3(), virtual_page, __pa(virtual_page), *page_entry, (unsigned long)page_entry, cccc->type, key);

        int _l;
        page_entry_t *_pte_ddd = get_pgeVaddr((unsigned long)(page_entry), &_l);
        printk(" --> L1 PTE of the entry PTE=0x%lx, (PTE_VA=0x%lx) level=%d\n",
                                    *_pte_ddd, (unsigned long)_pte_ddd, _l);
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