#ifndef _SVA_X86_H
#define _SVA_X86_H

/*
 * Chuqi: Given that all interfaces from the kernel
 * hashtable.h are basically macros and static inline 
 * functions.
 * 
 * Therefore, it is probably safe to just include the
 * header and steal them ;p
 */
#include <linux/hashtable.h>

/* 
 * Lazy static management. 
 * Let's assume the CVM has only few processes lol.
 * 8192 slots are enough for now.
 */
#define MAX_GLOB_VM_PROCESS    8192

/* use a global array, indexed by pid */
typedef struct encos_enclave_entry {
    /* assigned encid */
    int enc_id;
    /* Chuqi: 
     * We define an enclave is activated literally when it is ``active''.
     * Once LibOS's cleanup_and_call_elf_entry is called, the enclave is
     * program is executed.
     * 
     * When the process is executed, it is considered as ``dead''.
     */
    int activate;
} encos_enclave_entry_t;


extern int SM_encos_enclave_assign(void);
extern void SM_encos_enclave_claim_memory(unsigned long uva, 
                                          unsigned long pa, 
                                          unsigned long nr_pages, 
                                          int is_internalmem);
extern int SM_encos_enclave_act(int pid);
extern int SM_encos_enclave_exit(int pid);

extern void SM_encos_syscall_intercept(struct pt_regs *regs, int nr);
#endif