#include <sva/config.h>
#include <sva/enc.h>
#include <sva/mmu.h>
#include <sva/pks.h>
#include <sva/svamem.h>
#include <sva/stack.h>

#include <linux/sched.h>


#include <linux/string.h> 

#include <asm/current.h>
// #include <asm/proto.h>  /* SM_entry_SYSCALL_64 */
#include <asm/msr-index.h>
#include <asm/unistd.h>

#include <linux/smp.h>      /* smp_processor_id() */
#include <asm/current.h>    /* pcpu_hot */

#include <asm/special_insns.h> /* native_read_cr4 debug */

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

static void save_restore_pt_regs(struct pt_regs* dest, 
                                 const struct pt_regs *src)
{
    memcpy(dest, src, sizeof(struct pt_regs));
}

static encos_enclave_entry_t *current_enclave_entry(void) {
    if (unlikely(current->pid > MAX_GLOB_VM_PROCESS)) {
        log_err("Cannot find enc_id for pid=%d. Please adjust MAX_GLOB_VM_PROCESS.\n", current->pid);
        panic("GGWP!");
    }
    return &encos_enclave_table[current->pid];
}

int current_encid(void) {
    encos_enclave_entry_t *entry = current_enclave_entry();
    return entry->enc_id;
}


/* ==============================================================
 * PCPU stuff
 * ============================================================== */
#ifdef CONFIG_ENCOS_SYSCALL_STACK
#define SYS_STACK_SIZE  (0x1000)

/* Chuqi:
 * Enclave user to kernel (u2k) interface.
 * It should use an interposed syscall handler
 */

static void prepare_u2k_interface(int is_enclave)
{  
    /* enclave */
    if (is_enclave) {
        _wrmsr(MSR_LSTAR, (unsigned long)entry_SYSCALL_64_enclave);
    }
    /* normal */
    else {
        _wrmsr(MSR_LSTAR, (unsigned long)entry_SYSCALL_64);
    }
}

static void __this_pcpu_setup_syscall_stack(void *junk)
{
    unsigned long sys_stack_top;
    int cpu_id = smp_processor_id();

    sys_stack_top = (unsigned long)(SyscallSecureStackBase +
                                        SYS_STACK_SIZE * cpu_id);
    this_cpu_write(pcpu_hot.top_of_secure_sys_stack, sys_stack_top);
    /* Chuqi: remove debug */
    log_info("SyscallSecureStack setup for cpu=%d. top=0x%lx.\n", 
                cpu_id, sys_stack_top);
    return;
}

SECURE_WRAPPER(void,
SM_setup_pcpu_syscall_stack, void)
{
    int cpu;
    for_each_online_cpu(cpu) {
        printk(KERN_INFO "CPU %d is online\n", cpu);
    }

    on_each_cpu(__this_pcpu_setup_syscall_stack, 
                /*info=*/NULL, /*wait=*/1);
    return;
}


static inline unsigned long ROUNDUP(unsigned long address) {
    return (address + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
}
/* 
 * This should be called after switching to the secure syscall stack.
 * Since the caller is within SM code, it is not necessary to use 
 * the secure wrapper (entry and exit gates).
 */
void sm_validate_syscall_stack(unsigned long sys_rsp)
{
    unsigned long sys_stack_top;
    unsigned long sys_rsp_popq;
    int cpu_id = smp_processor_id();

    sys_rsp_popq = ROUNDUP(sys_rsp);
    sys_stack_top = (unsigned long)(SyscallSecureStackBase +
                                        SYS_STACK_SIZE * cpu_id);
    // printk("KERNEL CR4: 0x%lx.\n", native_read_cr4());
    /* Chuqi: remove debug */
    SVA_ASSERT(sys_rsp_popq == sys_stack_top, "Secure syscall stack validation failed.\n");

    if (sys_rsp_popq != sys_stack_top) {
        panic("[fault] ROUNDUP(sys_rsp)=0x%lx, sys_stack_top=0x%lx.\n", 
                ROUNDUP(sys_rsp), sys_stack_top);
    }
    return;
}

/*
 * SM controls the kernel to user path (e.g., exception returns).
 * Whenever switching into an activate enclave, SM should
 * 1) prepare the enclave u2k interface (e.g., syscall handler)
 */
SECURE_WRAPPER(void, 
SM_sched_in_userspace_prepare, struct pt_regs* regs)
{
    encos_enclave_entry_t *entry = current_enclave_entry();
    if (entry->activate) {
        prepare_u2k_interface(/*is_enclave=*/1);
    }
    /* normal process */
    else {
        prepare_u2k_interface(/*is_enclave=*/0);
    }
    return;
}
EXPORT_SYMBOL(SM_sched_in_userspace_prepare);
#endif

/* empty security monitor call */
SECURE_WRAPPER(void, 
SM_encos_empty, void)
{
    return;
}
EXPORT_SYMBOL(SM_encos_empty);

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
    
    /* declare its CR3 */
    encos_enclave_table[pid].enc_CR3 = __sm_read_cr3();
    page_desc_t *cr3_desc = getPageDescPtr(encos_enclave_table[pid].enc_CR3);
    cr3_desc->encID = enc_id;

    return enc_id;
}

SECURE_WRAPPER(void,
SM_encos_enclave_claim_memory, unsigned long uva,
unsigned long pa, unsigned long nr_pages,
int is_internalmem)
{
    int i;
    page_desc_t *page_desc;
    unsigned long kva;

    encos_enclave_entry_t *entry = current_enclave_entry();
    /* set up its CR3 */
    SVA_ASSERT(entry->enc_CR3, "The current enclave did not declare its CR3.\n");
    log_info("[pid=%d,enc_id=%d] Start claiming memory.{uva=0x%lx -> pa=0x%lx, nr_pages=%lu} is_internal=%d.\n", 
                current->pid, entry->enc_id, uva, pa, nr_pages, is_internalmem);
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
        /* 
         * mark those physical page descriptors 
         * as the enclave internal pages
         */
        for (i = 0; i < nr_pages; i++) {
            page_desc = getPageDescPtr(pa + i * pageSize);
            SVA_ASSERT(!pgIsActive(page_desc), "Claimed page (PA=0x%lx, type=%d) is not an unused page.\n",
                 (pa + i * pageSize), page_desc->type); 
            page_desc->type = PG_ENC;
            page_desc->encID = entry->enc_id;

            /* protect the kernel page */
            kva = (unsigned long)__va(pa + i * pageSize);
            set_page_protection(kva, /*should_protect=*/1);
        }
        /* Chuqi:
         * Add to the just claimed memory
         */
        entry->last_claim_mem.uva = uva;
        entry->last_claim_mem.pa = pa;
        entry->last_claim_mem.nr_pages = nr_pages;
    }
}

SECURE_WRAPPER(void,
SM_encos_enclave_protect_memory, 
unsigned long pa, unsigned long nr_pages)
{
    unsigned long kva;
    int i;
    for (i = 0; i < nr_pages; i++) {
        /* protect the kernel page */
        kva = (unsigned long)__va(pa + i * pageSize);
        set_page_protection(kva, /*should_protect=*/1);
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
SM_encos_vfork_child, int parent_pid, int child_pid)
{
    encos_enclave_entry_t *parent, *child;
    parent = &encos_enclave_table[parent_pid];
    child = &encos_enclave_table[child_pid];

    if (!parent->enc_id) {
        return;
    }
    SVA_ASSERT(!child->enc_id, "Cannot vfork child to an existed enclave child pid=%d.\n",
                child_pid);
    /* enclave id */
    child->enc_id = parent->enc_id;

    /* 
     * Check cr3. libOS's vfork should force the child to keep the same cr3 as the parent
     * and we enforce this case.
     */
    SVA_ASSERT(__sm_read_cr3() == parent->enc_CR3, 
        "vfork parent enclave CR3=0x%lx, child enclave CR3=0x%lx mismatch.\n",
        parent->enc_CR3, __sm_read_cr3());
    
    child->enc_CR3 = parent->enc_CR3;
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

SECURE_WRAPPER(void, SM_printvalues,
void *rdi, void *rsi, void *rdx)
{
    // return;
    printk("rdi=0x%lx, rsi=0x%lx, rdx=0x%lx.\n", 
                (unsigned long)rdi, (unsigned long)rsi, (unsigned long)rdx);
    return;
}

SECURE_WRAPPER(void, SM_save_restore_pt_regs,
struct pt_regs *regs,
void *target_stack,
unsigned int size)
{
    /* TODO: we don't obfuscate anything right now */
    printk("[pid=%d] pt_regs=0x%lx. orig_ax=0x%lx, copy_to_stack=0x%lx, cpy_size=%u.\n", 
                current->pid,
                (unsigned long)regs, regs->orig_ax, 
                (unsigned long)target_stack, (unsigned int)sizeof(struct pt_regs));
    /* Chuqi todo: mask and restore sensitive information */
    memcpy(target_stack, regs, sizeof(struct pt_regs));
    return;
}

SECURE_WRAPPER(void, SM_encos_syscall_enter, 
struct pt_regs* regs, int nr) {
    /*
     * Once an enclave is activated, we 
     * intercept its syscalls (enter) into here.
     */
    encos_enclave_entry_t *entry;
    unsigned long arg0, arg1, arg2, arg3, arg4, arg5;

    entry = current_enclave_entry();
    // if (!entry || !entry->activate) {
    //     return;
    // }

    save_restore_pt_regs(&entry->pt_regs, regs);

    arg0 = regs->di;
    arg1 = regs->si;
    arg2 = regs->dx;
    arg3 = regs->r10;
    arg4 = regs->r8;
    arg5 = regs->r9;

    log_info("[enc_pid=%d] Intercept syscall_enter[%d].\n", 
              current->pid, nr);
    
    /* mmap */
    switch (nr)
    {
        case __NR_mmap:
            break;
        default:
            break;
    }
}

SECURE_WRAPPER(void, SM_encos_syscall_return, 
struct pt_regs* regs, int nr) {
    /*
     * Once an enclave is activated, we 
     * intercept its syscalls (ret) into here.
     */
    encos_enclave_entry_t *entry;
    unsigned long arg0, arg1, arg2, arg3, arg4, arg5;
    unsigned long ret;
    
    entry = current_enclave_entry();
    // if (!entry || !entry->activate) {
    //     return;
    // }

    ret = regs->ax;
    arg0 = regs->di;
    arg1 = regs->si;
    arg2 = regs->dx;
    arg3 = regs->r10;
    arg4 = regs->r8;
    arg5 = regs->r9;

    log_info("[enc_pid=%d] Intercept syscall_return[%d].\n", 
              current->pid, nr);
    
    /* mmap */
    switch (nr)
    {
        case __NR_mmap:
            // SM_mmap_return(ret, arg1, (int)arg4, arg5);
            break;
        
        default:
            break;
    }
}