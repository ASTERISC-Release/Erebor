#ifndef SVA_PKS_H
#define SVA_PKS_H

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
#ifdef CONFIG_ENCOS_PKS
    if (cpu_feature_enabled(X86_FEATURE_PKS))
        return;
    else
        panic("PKS is not available.\n");
#endif

#ifdef CONFIG_ENCOS_WP
    /* for write protection we assume its always available */
    return;
#endif
    return;
}

extern void set_page_protection(uintptr_t virtual_page, int should_protect);

/* chuqi: this is changed to `set_page_protection` */
// extern void pks_update_mapping(uintptr_t, int);
extern void pks_set_key(int, bool, bool);

#endif // SVA_PKS_H