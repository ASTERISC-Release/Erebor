#include <sva/config.h>
#include <sva/enc.h>
#include <sva/svamem.h>
#include <sva/stack.h>

#include <asm/current.h>
#include <linux/sched.h>

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
        panic("GG!");
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
    encos_enclave_entry_t entry = encos_enclave_table[enc_pid];

    log_info("Start claiming memory. enc_id=%d, is_internal=%d.\n", 
                entry.enc_id, is_internalmem);
    /* Chuqi: 
     * for enclave internal memory, we should mark and
     * check their page table entries & page descriptors
     */
    if (is_internalmem) {
        /* sanity check */
        if (!entry.enc_id) {
            log_err("Cannot claim internal memory for a non-enclave pid=%d.\n",
                     current->pid);
            panic("GG!");
        }
        /* TODO:
         * mark those physical page descriptors 
         * as the enclave internal pages
         */
        /* TODO:
         * check their uva -> pa mapping in the page table
         */
    }
    /* Chuqi: 
     * for enclave inter-container shared memory, we should mark and
     * ensure no user container has write permissions
     */
    else {
        if (entry.enc_id) {
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
        panic("GG!");
    }
    if (entry->activate) {
        log_err("Cannot activate enc_id=%d for pid=%d. It is already activated.\n", 
                 entry->enc_id, pid);
        panic("GG!");
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
    /* sanity checks */  
    if (!entry->activate) {
        log_err("Cannot exit enc_id=%d for pid=%d. It is not activated.\n", 
                 entry->enc_id, pid);
        panic("GG!");
        return -1;
    }
    entry->enc_id = 0;
    entry->activate = 0;

    log_info("Exited enc_id=%d pid=%d.\n", entry->enc_id, pid);

    return 0;
}


SECURE_WRAPPER(void, SM_encos_syscall_intercept, 
struct pt_regs* regs, int nr) {
  log_info("[enc_pid=%d] Intercept syscall=%d.\n", 
            current->pid, nr);
  // if its mmap
  // if its enclave activated

  /* 
   * 1. Try to iterate the (UVA, size) mappings in the page table.
   * If found, then make sure there is no write permission bits
   */

  /* 
   * 2. If UVA mappings are not found.
   * We make the (pid, UVA) to be checked during page_fault+mmu mappings
   */
}