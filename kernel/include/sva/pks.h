#ifndef SVA_PKS_H
#define SVA_PKS_H

#include <sva/config.h>
#include <asm/cpufeature.h>
#include <linux/kernel.h>

/* Intel-defined CPU features, CPUID level 0x00000007:0 (ECX), word 16 */
#ifndef X86_FEATURE_PKS
#define X86_FEATURE_PKS	(16*32+31) /* Protection Keys for Supervisor pages */
#endif

#ifndef MSR_IA32_PKRS
#define MSR_IA32_PKRS			0x000006E1
#endif

static inline void check_protection_available(void)
{
    if (cpu_feature_enabled(X86_FEATURE_PKS)) {
        log_info("Protection Key, Supervisor (PKS) feature check: passed.\n");
        return;
    }
    else
        panic("PKS is not available.\n");

    return;
}

static inline int __check_pte_protection(unsigned long *pte)
{
    /* PKS key 0 means the page is unprotected */
    if(((*pte >> 59) & 0xf) == 0)
        return 0;
    return 1;
}

/* Chuqi: 
 * Please not flush tlb is called after __set_pte_protection,
 * which is during __do_mmu_update.
 */
static inline void __set_pte_protection(unsigned long *pte, int should_protect)
{
// #ifdef CONFIG_ENCOS_PKS
    /*
     * For the PKS version, we always use PTE.key=1 for the protected page.
     * key=0 is reserved for the kernel (unprotected pages).
     */
    if (should_protect)
        *pte |= (0ull << 59);
    else
        /* set the existing key in the PTE to 0 */
        *pte &= (0x87FFFFFFFFFFFFFF);
// #elif defined(CONFIG_ENCOS_WP)
//     /*
//      * For the WP version, we always use PTE.RW=0 for the protected page.
//      * RW=1 means the page is writable to kernel (unprotected pages).
//      */
//     if (should_protect)
//         *pte &= ~PG_RW;
//     else
//         *pte |= PG_RW;
// #endif
}

extern int set_page_protection(uintptr_t virtual_page, int should_protect);

/* chuqi: this is changed to `set_page_protection` */
// extern void pks_update_mapping(uintptr_t, int);
// extern void pks_set_key(int, bool, bool);

#endif // SVA_PKS_H