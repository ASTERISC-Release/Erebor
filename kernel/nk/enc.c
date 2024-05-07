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

#ifdef ENCOS_DEBUG
    log_info("Assigned enc_id=%d for pid=%d.\n", enc_id, pid);
#endif

    encos_enclave_table[pid].enc_id = enc_id;
    encos_enclave_table[pid].activate = 0;
    return enc_id;
}

/* activate */
SECURE_WRAPPER(void, 
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
    }
    entry->activate = 1;
#ifdef ENCOS_DEBUG
    log_info("Activated enc_id=%d pid=%d.\n", entry->enc_id, pid);
#endif
    return;
}

/* process exit */
SECURE_WRAPPER(void, 
SM_encos_enclave_exit, int pid)
{
    encos_enclave_entry_t *entry;
    entry = &encos_enclave_table[pid];
    /* not an enclave. */
    if (!entry->enc_id) {
        return;
    }
    /* sanity checks */  
    if (!entry->activate) {
        log_err("Cannot exit enc_id=%d for pid=%d. It is not activated.\n", 
                 entry->enc_id, pid);
        panic("GG!");
    }
    entry->enc_id = 0;
    entry->activate = 0;
#ifdef ENCOS_DEBUG
    log_info("Exited enc_id=%d pid=%d.\n", entry->enc_id, pid);
#endif
    return;
}
