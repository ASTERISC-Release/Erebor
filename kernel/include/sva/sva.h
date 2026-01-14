#ifndef _SVA_SVACALLS
#define _SVA_SVACALLS

// MMU Operations
#define SVA_MMU_INIT                                    0

#define SVA_DECLARE_L1                                  1
#define SVA_DECLARE_L2                                  2
#define SVA_DECLARE_L3                                  3
#define SVA_DECLARE_L4                                  4
#define SVA_DECLARE_L5                                  5

#define SVA_UPDATE_L1                                   6
#define SVA_UPDATE_L2                                   7
#define SVA_UPDATE_L3                                   8
#define SVA_UPDATE_L4                                   9
#define SVA_UPDATE_L5                                   10

#define SVA_REMOVE_PAGE                                 11
#define SVA_REMOVE_PAGES                                12
#define SVA_REMOVE_MAPPING                              13

#define SVA_STACK_TEST                                  14
#define SVA_MM_LOAD_PAGETABLE                           15

// CSR + MSR Operations
#define SVA_WRITE_CR0                                   16
#define SVA_WRITE_CR4                                   17
#define SVA_WRITE_MSRL                                  18
#define SVA_BENCH_ENCOS_WRITE_MSRL                      19

// Misc
#define SVA_SECURE_POKE                                 20
#define SVA_MEMCPY                                      21
#define SVA_CLEAR_PAGE                                  22
#define SVA_COPY_USER                                   23

// ENCLAVE Operations
#define SVA_SETUP_PCPU_CALL_STACK                       24
#define SVA_SCHED_IN_USERSPACE_PREPARE                  2
#define SVA_ENCOS_EMPTY                                 26
#define SVA_ENCOS_ENCLAVE_ASSIGN                        27
#define SVA_ENCOS_ENCLAVE_CLAIM_MEMORY                  28
#define SVA_ENCOS_ENCLAVE_PROTECT_MEMORY                29
#define SVA_ENCOS_ENCLAVE_ACT                           30
#define SVA_ENCOS_ENCLAVE_EXIT                          31
#define SVA_ENCOS_ENCLAVE_VFORK_CHILD                   32
#define SVA_ENCOS_ENCLAVE_PRINTVALUES                   33
#define SVA_ENCOS_ENCLAVE_SAVE_RESTORE_PT_REG           34
#define SVA_ENCOS_SYSCALL_ENTER                         35
#define SVA_ENCOS_SYSCALL_RETURN                        36

#define SVA_TDCALL                                      37

#endif