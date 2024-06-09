#include <asm/msr.h>
#include <sva/mmu.h>
#include <sva/svamem.h>
#include <sva/pks.h>
#include <sva/stack.h>
#include <sva/mmu_intrinsics.h>
#include <linux/slab.h>
#include <asm/pgtable.h>
#include <asm/set_memory.h>

// in total 16MB
#define N_INTERNAL_PAGES    0x1000
// a page = 0x1000 bytes = 4KB; 1 bytes = 8 bits = void

/*
 * Chuqi: 
 * The internal_split_PTPs are static pages that are used 
 * 
 * Please not that we cannot indicate their attribute as SVA page here.
 * Otherwise, these PTP's page type will become PAGE_SVA during 
 * mmu_init() -> init_protected_pages(_svastart, _svaend, PG_SVA);
 * 
 * Therefore, we mark them as normal statci data pages rn and protect them.
 */
static char internal_split_PTPs[N_INTERNAL_PAGES][0x1000] __attribute__((aligned(0x1000)));

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

#ifdef CONFIG_ENCOS
int set_page_protection(unsigned long virtual_page, int should_protect)
{   
    int level = 0;
    page_entry_t *page_entry = get_pgeVaddr(virtual_page, &level);
    // printk("pks set_page_protection: START level=%d; entryVA=0x%lx, entry=0x%lx, type: %d, key = %d\n", 
    //     level, (unsigned long)page_entry, *(unsigned long*)page_entry, getPageDescPtr(__pa(virtual_page))->type, key);

    /*
     * early exit if the target page is not in the protection
     * domain and we don't want to protect it.
     * 
     * in PKS version:
     * virtual_page's PTE(key) == 0 && key (to be set) == 0; 
     * this is to avoid unnecessary page splits.
     * 
     * in WP version:
     * virtual_page's PTE(R/W) == 1 && should_protect == 0;
     */ 
    if (!__check_pte_protection(page_entry) && !should_protect) {
        return 0;
    }

    /* 
     * In case that the virtual_page (to be protected) is mapped by a L2-PTE (hugepage),
     * we need to split the hugepage into 512 * 4KB pages by:
     * (1) allocate a L1-PTP,
     * (2) use L1-PTP's 512 PTE entries to point to the 512 * 4KB pages (split),
     * (3) point the original L2-PTE to the allocated L1-PTP,
     * (4) set the protection key in the corresponding L1-PTE of the target virtual_page,
     * (5) set the protection key in the corresponding L1-PTE of the L1-PTP's virtual page.
     */
    /* Tightly copupling the split page function with pks.c for now */
    if(level == 2) {
        /* allocate a 4KB page as the L1_PTP */
        void *l1_ptp_page = (void *)__internal_alloc_PTP();
        if (!l1_ptp_page) {
            printk(KERN_ERR "Failed to allocate memory\n");
            return -ENOMEM;
        }
        
        /* Update the metadata for the L2 and L1 PTPs */
        /* 
         * Cannot call a sva_declare_l2_page_secure here, since 
         * it would attempt to re-acquire a the MMULock. 
         * Hence, we update the page_desc manually.
         */
        page_desc_t *pgDescL1 = getPageDescPtr(__pa(l1_ptp_page));
        memset(pgDescL1, 0, sizeof(struct page_desc_t));
        pgDescL1->type = PG_L1;
        /* todo should we set up this pgVaddr? seems useless */
        // pgDescL1->pgVaddr = get_page_entry();
        pgDescL1->count = 1;

        page_desc_t *pgDescL2 = getPageDescPtr(__pa(((unsigned long)page_entry & 0xFFFFFFFFFFFFF000)));
        pgDescL2->type = PG_L2;

        /* get the correct PTE flags for the L1 mappings */
        pgprot_t ref_prot;
		ref_prot = pmd_pgprot(*(pmd_t *)page_entry);
		ref_prot = pgprot_large_2_4k(ref_prot);
        ref_prot = pgprot_clear_protnone_bits(ref_prot);

        /* add mappings from the allocated L1_PTP to the 512x(4KB) pages */
        uintptr_t *l1_pte;
        for(int i = 0; i < 512; i++) {
            l1_pte = (uintptr_t *)(l1_ptp_page + i * 8);
            uintptr_t pte = pfn_pte(((*page_entry + i * PAGE_SIZE) >> 12), ref_prot).pte;
            
            // TODO debug print here
            // if(i == ((virtual_page >> (12 - 3)) & 0xfff)) {
            //     printk("[before settings] set page protection: CR3=0x%lx, page (va=0x%lx, pa=0x%lx, entry=0x%lx, entryVA=0x%lx, type=%d) with key=%d (splitted idx=%d)\n", 
            //         read_cr3(), virtual_page, __pa(virtual_page), 
            //         *(unsigned long*)l1_pte, (unsigned long)l1_pte, 
            //         getPageDescPtr(__pa(virtual_page))->type, key, i);
            
            //     int _l;
            //     page_entry_t *_pte_ddd = get_pgeVaddr((unsigned long)l1_pte, &_l);
            //     printk(" --> L1 PTE of the entry PTE=0x%lx, (PTE_VA=0x%lx) level=%d\n",
            //                             *_pte_ddd, (unsigned long)_pte_ddd, _l);
            // }

            *l1_pte = pte;

            /* only protect the sensitive pte (l1_pte of the virtual_page) */
            if(i == ((virtual_page >> (12 - 3)) & 0xfff))
                __set_pte_protection(l1_pte, should_protect);
        }

        /* Point the splitted L2 page entry (PTE) --to--> the newly created L1_PTP page */
        unsigned long l1_ptp_page_nr = (unsigned long)__pa(l1_ptp_page) >> 12;
        struct pgprot _pgprot = __pgprot(_KERNPG_TABLE);
        *page_entry = pfn_pte(l1_ptp_page_nr, _pgprot).pte;

        /* protect the created l1_ptp page */
        set_page_protection((unsigned long)l1_ptp_page, /*should_protect=*/1);

        /* debug print */
        // int index = (virtual_page >> (12-3)) & 0xfff;
        // int _ll;
        // page_entry_t *_pte_xxx = get_pgeVaddr((unsigned long)(l1_ptp_page + index * 8), &_ll);
        // printk("[FINAL] CR3=0x%lx --> L1 PTE of the entry PTE=0x%lx, (PTE_VA=0x%lx) level=%d\n",
        //                         read_cr3(), *_pte_xxx, (unsigned long)_pte_xxx, _ll);
    } else if(level == 1) {
        /* set protection key in PTE */
        __set_pte_protection(page_entry, /*should_protect=*/should_protect);
        
        /* debug print */
        // printk("set page protection: CR3=0x%lx, page (va=0x%lx, pa=0x%lx, entry=0x%lx, entryVA=0x%lx, type=%d) with key=%d\n", 
        //         read_cr3(), virtual_page, __pa(virtual_page), *page_entry, (unsigned long)page_entry, getPageDescPtr(__pa(virtual_page))->type, key);
        // int _l;
        // page_entry_t *_pte_ddd = get_pgeVaddr((unsigned long)(page_entry), &_l);
        // printk(" --> L1 PTE of the entry PTE=0x%lx, (PTE_VA=0x%lx) level=%d\n",
        //                             *_pte_ddd, (unsigned long)_pte_ddd, _l);
    }
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
#else
int set_page_protection(unsigned long virtual_page, int should_protect) {}
#endif