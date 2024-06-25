#ifndef _SVA_X86_H
#define _SVA_X86_H

#include <asm/ptrace.h>  /* for struct pt_regs */

#ifdef __ASSEMBLY__
/* Chuqi:
 * The following interfaces are exposed to entry_64.S
 * to interpose the system call table.
 */

.extern SM_printvalues(void *rdi, void *rsi, void *rdx);

.extern SM_tdcall(void);
.extern SM_tdcall_nowrap(void);

.extern sm_validate_syscall_stack(unsigned long sys_rsp);

.extern SM_encos_syscall_enter(struct pt_regs *regs, int nr);
.extern SM_encos_syscall_return(struct pt_regs *regs, int nr);
.extern SM_save_restore_pt_regs(struct pt_regs *regs, void *target_stack, unsigned int size);

.macro SM_ENCOS_SYSCALL_ENTER
	pushq	%rdi
	pushq	%rsi
	call	SM_encos_syscall_enter
	popq	%rsi
	popq	%rdi
	pushq	%rdi
	pushq	%rsi
.endm

.macro SM_ENCOS_SYSCALL_RETURN
    popq	%rsi
	popq	%rdi
	pushq	%rdi
	pushq	%rsi
	call	SM_encos_syscall_return
	popq	%rsi
	popq	%rdi
.endm

#else

/* 
 * Lazy static management. 
 * Let's assume the CVM has only few processes lol.
 * 16384 slots are enough for now.
 */
#define MAX_GLOB_VM_PROCESS    65536 //16384

/* use a global array, indexed by pid */
typedef struct encos_enclave_last_claim_mem {
    unsigned long uva;
    unsigned long pa;
    int nr_pages;
} encos_enclave_last_claim_mem_t;


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
    /* 
     * Just claimed enclave internal memory in encos_mmap.
     * This is used for the mmap double check.
     */
    encos_enclave_last_claim_mem_t last_claim_mem;
    /* Chuqi:
     * We should save the context here
     */
    struct pt_regs pt_regs;
    /* Chuqi:
     * We only allow enclave trigger pagefault during mmap
     */
    int in_mmap;

    unsigned long enc_CR3;
} encos_enclave_entry_t;

/* internal */
extern int current_encid(void);


#ifdef CONFIG_ENCOS_SYSCALL_STACK
extern void SM_setup_pcpu_syscall_stack(void);
#endif

/* empty */
extern void SM_encos_empty(void);

extern void SM_sched_in_userspace_prepare(struct pt_regs* regs);

extern int SM_encos_enclave_assign(void);
extern void SM_encos_enclave_claim_memory(unsigned long uva, 
                                          unsigned long pa, 
                                          unsigned long nr_pages, 
                                          int is_internalmem);
extern void SM_encos_enclave_protect_memory(unsigned long pa, 
                                            unsigned long nr_pages);
extern int SM_encos_enclave_act(int pid);
extern int SM_encos_enclave_exit(int pid);

extern void SM_encos_vfork_child(int parent_pid, int child_pid);

extern int stac_bool;
extern int stac_map[60];

#endif /* __ASSEMBLY__ */
#endif /* _SVA_X86_H */