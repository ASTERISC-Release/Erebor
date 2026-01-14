#ifndef SVA_MSR_H
#define SVA_MSR_H
/*
 *****************************************************************************
 * Low level register read/write functions
 *****************************************************************************
 */
#include <linux/panic.h>

#define MSR_REG_EFER    0xC0000080      /* MSR for EFER register */
#define MSR_REG_PKRS    0x6e1           /* MSR for PKRS register */
/* CR0 Flags */
#define     CR0_WP      0x00010000      /* Write protect enable */

/* CR4 Flags */
#define     CR4_SMEP    0x00100000      /* SMEP enable */
#define     CR4_PKS     (1ul << 24)     /* PKS enable */
/* EFER Flags */
#define     EFER_NXE    0x00000800     /* NXE enable */


/*
 * Function: encos_write_msr
 *
 * Description:
 *  It is not necessary to wrap this with the secure call gate, given the
 *  security checks after wrmsr.
 */
#define ENCOS_MSR_ASSERT(truth, fmt, args...)   \
    if(!(truth)) {\
        panic(fmt, ## args); \
    }

static __always_inline void __wrmsr_check(unsigned int msr, unsigned long long val)
{
    ENCOS_MSR_ASSERT(!((msr == MSR_REG_EFER) && !(val & EFER_NXE)),
		  "ENCOS: attempt to clear the EFER.NXE bit: 0x%llx.", val);

  	/* Chuqi: todo: add PKRS check back */
    // ENCOS_MSR_ASSERT(msr != MSR_REG_PKRS, "ENCOS: OS attempts to write to PKRS.");
}

extern void sva_write_cr0 (unsigned long val);
extern void sva_write_cr4 (unsigned long val);

extern void encos_write_msrl(unsigned int msr, unsigned long val);

static __always_inline void encos_write_msr_boot(unsigned int msr, unsigned int low, unsigned int high)
{
    unsigned long long val;
    if (msr == MSR_REG_EFER) {
        low |= EFER_NXE;
    }
    val = (((unsigned long long)high) << 32) | low;
    
    asm volatile("wrmsr" : : "c" (msr), "a"(low), "d" (high) : "memory");
    __wrmsr_check(msr, val);
}

static __always_inline void encos_write_msr(unsigned int msr, unsigned int low, unsigned int high)
{
    unsigned long long val;
    if (msr == MSR_REG_EFER) {
        low |= EFER_NXE;
    }
    val = (((unsigned long long)high) << 32) | low;
    
    asm volatile("1: wrmsr\n"
			  "2:\n"
			  _ASM_EXTABLE_TYPE(1b, 2b, EX_TYPE_WRMSR)
		: : "c" (msr), "a"(low), "d" (high) : "memory");

    __wrmsr_check(msr, val);
}

static inline int encos_write_msr_safe(unsigned int msr, unsigned int low, unsigned int high)
{
	int err;
	unsigned long long val;
	if(msr == MSR_REG_EFER) {
		low |= EFER_NXE;
	}
	val = ((unsigned long long)high << 32) | low;

	asm volatile("1: wrmsr ; xor %[err],%[err]\n"
		  "2:\n\t"
		  _ASM_EXTABLE_TYPE_REG(1b, 2b, EX_TYPE_WRMSR_SAFE, %[err])
		  : [err] "=a" (err)
		  : "c" (msr), "0" (low), "d" (high)
		  : "memory");

	/*
	 * ENCOS: the outer kernel will always execute the below security checks.
	 */
	__wrmsr_check(msr, val);
    return err;
}

#endif  /* SVA_MSR_H */