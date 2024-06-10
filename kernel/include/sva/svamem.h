#ifndef _SVAMEM_H_
#define _SVAMEM_H_

// asm(".section svamem, \"aw\", @nobits");
#define SVAMEM __attribute__((section(".svamem.data")))

/* General SM text section */
#define SVATEXT __attribute__((section(".svamem.text")))
#define __svatext   __section(".svamem.text")

/* 
 * SM text section with all sensitive instructions (wrmsr, mov cr0-cr4).
 * Besides memory protection (PKS / WP), we should also unmap this 
 * section when at the untrusted OS.
 */
#define SVATEXT_PRIV __attribute__((section(".svamem.priv.text")))
#define __svatext_priv   __section(".svamem.priv.text")

#endif // _SVA_MEM_H_

