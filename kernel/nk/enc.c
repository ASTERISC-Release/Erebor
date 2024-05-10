#include <sva/config.h>
#include <sva/enc.h>
#include <sva/svamem.h>
#include <sva/stack.h>

#include <asm/current.h>
#include <linux/sched.h>

#include <asm/unistd.h>

#ifndef PAGE_SIZE
#define PAGE_SIZE   4096
#endif
/* management structure */
static encos_enclave_entry_t encos_enclave_table[MAX_GLOB_VM_PROCESS] SVAMEM;

static int g_enc_id = 1;

static inline int _assign_enc_id(void) {
    /* TODO: increase value */
    return g_enc_id;
}

/* assign a fresh enclave id for the process */
SECURE_WRAPPER(int, 
SM_encos_enclave_assign, void)
{
    int pid, enc_id;
    pid = current->pid;
    enc_id = _assign_enc_id();
    
    if (pid > MAX_GLOB_VM_PROCESS) {
        log_err("Cannot assign enc_id for pid=%d. Please adjust MAX_GLOB_VM_PROCESS.\n", pid);
        panic("GGWP!");
    }

    log_info("Assigned enc_id=%d for pid=%d.\n", enc_id, pid);

    encos_enclave_table[pid].enc_id = enc_id;
    encos_enclave_table[pid].activate = 0;
    return enc_id;
}

SECURE_WRAPPER(void,
SM_encos_enclave_claim_memory, unsigned long uva,
unsigned long pa, unsigned long nr_pages,
int is_internalmem)
{
    int enc_pid = current->pid;
    encos_enclave_entry_t *entry = &encos_enclave_table[enc_pid];

    log_info("[pid=%d,enc_id=%d] Start claiming memory.{uva=0x%lx -> pa=0x%lx, nr_pages=%lu} is_internal=%d.\n", 
                enc_pid, entry->enc_id, uva, pa, nr_pages, is_internalmem);
    /* Chuqi: 
     * for enclave internal memory, we should mark and
     * check their page table entries & page descriptors
     */
    if (is_internalmem) {
        /* sanity check */
        if (!entry->enc_id) {
            log_err("Cannot claim internal memory for a non-enclave pid=%d.\n",
                     current->pid);
            panic("GGWP!");
        }
        /* TODO:
         * mark those physical page descriptors 
         * as the enclave internal pages
         */
        /* TODO:
         * check their uva -> pa mapping in the page table
         */
        /* Chuqi:
         * Add to the just claimed memory
         */
        entry->last_claim_mem.uva = uva;
        entry->last_claim_mem.pa = pa;
        entry->last_claim_mem.nr_pages = nr_pages;
    }
    /* Chuqi: 
     * for enclave inter-container shared memory, we should mark and
     * ensure no user container has write permissions
     */
    else {
        if (entry->enc_id) {
            /* revoke the W permissions in page table entries */
        }
    }
}

/* activate */
SECURE_WRAPPER(int, 
SM_encos_enclave_act, int pid)
{
    encos_enclave_entry_t *entry;
    entry = &encos_enclave_table[pid];
    /* sanity checks */    
    if (!entry->enc_id) {
        log_err("Cannot activate enc_id for pid=%d. Please assign enc_id first.\n", pid);
        panic("GGWP!");
    }
    if (entry->activate) {
        log_err("Cannot activate enc_id=%d for pid=%d. It is already activated.\n", 
                 entry->enc_id, pid);
        panic("GGWP!");
        return -1;
    }
    entry->activate = 1;

    log_info("Activated enc_id=%d pid=%d.\n", entry->enc_id, pid);

    return entry->enc_id;
}

/* process exit */
SECURE_WRAPPER(int, 
SM_encos_enclave_exit, int pid)
{
    encos_enclave_entry_t *entry;
    entry = &encos_enclave_table[pid];
    /* not an enclave. */
    if (!entry->enc_id) {
        return -1;
    }
    entry->enc_id = 0;
    entry->activate = 0;

    log_info("Exited enc_id=%d pid=%d.\n", entry->enc_id, pid);

    return 0;
}

SECURE_WRAPPER(void,
SM_encos_populate_child, int parent_pid, int child_pid)
{
    encos_enclave_entry_t *parent, *child;
    parent = &encos_enclave_table[parent_pid];
    child = &encos_enclave_table[child_pid];

    if (!parent->enc_id) {
        return;
    }
    if (child->enc_id) {
        log_err("Cannot populate child for an existed enclave child pid=%d.\n",
                 child_pid);
        panic("GGWP!");
    }
    child->enc_id = parent->enc_id;
    child->activate = 0;

    log_info("Populated child enc_id=%d pid=%d.\n", parent->enc_id, child_pid);
}

/* ===================================================
 * SYSCALL
 * =================================================== */

/* 
 * mmap: 
 * @uva: mapped userspace virtual address
 * @len: length of the mapping
 * @fd: file descriptor
 * @offset: offset in the file
 */
static inline int SM_mmap_return(unsigned long uva, unsigned long len, 
                                 int fd, unsigned long offset)
{
    encos_enclave_entry_t *entry;
    entry = &encos_enclave_table[current->pid];

    log_info("[enc_pid=%d] mmap ret(addr=0x%lx). arg (len=0x%lx,fd=%d,offset=0x%lx).\n", 
                  current->pid, uva, len, fd, offset);
    /* Chuqi: 
     * 1. If we just claimed the memory as the enclave internal
     * memory (during encos_mmap), then it is safe to ignore.
     *
     * Because we already validate its uva->pa mappings, as the 
     * `SM_encos_enclave_claim_memory` is invoked just now.
     * 
     * If the adversary OS tried to bypass memory claiming, we 
     * will catch this and then revoke the write permissions here,
     * because `SM_encos_enclave_claim_memory` is not called and 
     * `encos_enclave_last_claim_mem_t` is not set for that proc.
     */
    if (uva == entry->last_claim_mem.uva && 
        len <= entry->last_claim_mem.nr_pages * PAGE_SIZE) {
        /* check pass. */
        log_info("Ignore mmap for claimed memory. uva=0x%lx, len=0x%lx.\n", uva, len);
        return 0;
    }
    if (fd == -1) {
        /* ignore the stupid userspace FUTEX claim for now */
        panic("WTF who allowed you to use anonymous mappings?????");
    }
        
    /* 2. Else, we should properly handle the page permissions */
    /* 
     * 1. Try to iterate the (UVA, size) mappings in the page table.
     * If found, then make sure there is no write permission bits
     */

    /* 
     * 2. If UVA mappings are not found.
     * We make the (pid, UVA) to be checked during page_fault+mmu mappings
     */
    return 0;
}

SECURE_WRAPPER(void, SM_encos_syscall_intercept, 
struct pt_regs* regs, int nr) {
    /*
     * Once an enclave is activated, we 
     * intercept its syscalls (ret) into here.
     */
    unsigned long arg0, arg1, arg2, arg3, arg4, arg5;
    unsigned long ret = regs->ax;
    arg0 = regs->di;
    arg1 = regs->si;
    arg2 = regs->dx;
    arg3 = regs->r10;
    arg4 = regs->r8;
    arg5 = regs->r9;

    log_info("[enc_pid=%d] Intercept syscall=%d.\n", 
              current->pid, nr);
    
    /* mmap */
    switch (nr)
    {
        case __NR_mmap:
            SM_mmap_return(ret, arg1, (int)arg4, arg5);
            break;
        
        default:
            break;
    }
}