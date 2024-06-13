#include "common.h"

#include <linux/mm.h>
#include <linux/cma.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/sched.h>
#include <linux/debugfs.h>
#include <linux/io.h>
#include <linux/moduleparam.h>
#include <linux/delay.h>
#include <linux/highmem.h>
#include <asm/pgtable.h>
#include <linux/kallsyms.h>
#include <linux/fs.h>

#include <sva/enc.h>
#include <linux/encos.h>
#include "encos_alloc.h"

// chuqi: micro perf
#include "encos_perf.h"

struct miscdevice *misc;
struct mutex encos_dev_mlock;

int encos_kdbg_enabled = 0;

/* Chuqi:
 * Note that this is the untrusted copy of enclave info on the system.
 * This is just for lazy implementation (rn for syscall intercepting)
 * 
 * See sva/enc.h for the maintained trusted version.
 */
/* use a global array, indexed by pid */
// #define MAX_GLOB_VM_PROCESS    65536 // 16384
// typedef struct encos_enclave_entry_ut {
//     /* assigned encid */
//     int enc_id;
//     int activate;
// } encos_enclave_entry_ut_t;
// static encos_enclave_entry_ut_t encos_enclave_table_ut[MAX_GLOB_VM_PROCESS];

// int is_enclave_activate_ut(int pid)
// {
//     return (&encos_enclave_table_ut[pid])->activate;
// }
// EXPORT_SYMBOL(is_enclave_activate_ut);

// void free_enclave_ut(int pid)
// {
//     (&encos_enclave_table_ut[pid])->enc_id = 0;
//     (&encos_enclave_table_ut[pid])->activate = 0;
// }
// EXPORT_SYMBOL(free_enclave_ut);

static long encos_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int rvl;
    int enc_pid, enc_pgrp, enc_id;
    rvl = 0;
    switch (cmd) {
        case ENCOS_ENCLAVE_REQUEST:
            enc_pid = current->pid;
            enc_pgrp = task_pgrp_nr(current);
            enc_id = SM_encos_enclave_assign();
            log_info("[Assign enclave] pid=%d, pgrp=%d enc_id=%d.\n", 
                        enc_pid, enc_pgrp, enc_id);
            // (&encos_enclave_table_ut[enc_pid])->enc_id = enc_id;
            break;
        
        case ENCOS_ENCLAVE_ACT:
            enc_pid = current->pid;
            rvl = SM_encos_enclave_act(enc_pid);
            // if (rvl > 0) {
            //     (&encos_enclave_table_ut[enc_pid])->activate = 1;
            // }
            break;
        
        case ENCOS_ENCLAVE_EXIT:
            SM_encos_enclave_exit(current->pid);
            break;
        /*
         * Kernel debug logging enable/disable
         */
        case ENCOS_ENABLE_KDBG:
            encos_kdbg_enabled = 1;
            break;

        case ENCOS_DISABLE_KDBG:
            encos_kdbg_enabled = 0;
            break;
    }
    return (long)rvl;
}

/** ==================================================
 * ENCOS memory mmap() interface
 * =================================================== */
/** Interface for ENCOS's mmap.
 * 
 * This mmap() is only used for memory map management for enclave's internal & shared memory.
 * 
 * userspace: mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
 * or kern: ksys_mmap_pgoff(unsigned long addr, unsigned long len,
 *                          unsigned long prot, unsigned long flags,
 *                          unsigned long fd, unsigned long pgoff);
 * 
 * The @param offset / pgoff is used to control the behavior of this mmap.
 * (1) When vma->vm_pgoff = (0 || 0xbabe): Allocate a physical memory chunk for the enclave (do allocation + mmap)
 * (2) else vma->vm_pgoff > 0: Map a physical page starting from pg_off for the enclave (only do anonymous mapping)
 * 
 * Case (2) is unlikely happened.
 */
static int encos_mmap(struct file *file, struct vm_area_struct *vma)
{
    unsigned long start, size, pg_off;
    struct page *page;
    struct page **pages;
    int i, rvl;
    int enc_id;
    unsigned long num;

    encos_shmem_hash_entry_t *shmem_entry;

    bool is_internalmem, is_futex;
    encos_mem_t *enc_mem;
    unsigned long base_phys_addr, phys_pfn;

    start = vma->vm_start;
    size = vma->vm_end - vma->vm_start;
    pg_off = vma->vm_pgoff;
    /* Chuqi: 
     * `babe` you are the libOS's internal futex allocation request :p 
     * Note that the libOS uses the OFFSET=0xbabe to distinguish futex
     * allocation request.
     * 
     * We need some "secret sauce" to deal with futex memory. See 
     * `encos_alloc`.
     */
    is_internalmem = (pg_off == 0 || pg_off == 0xbabe) ? true : false;
    is_futex = (pg_off == 0xbabe);

    /* allocate an ``internal'' physical memory chunk */
    if (is_internalmem) {  /* alloc + mmap */
        if ((enc_mem = encos_alloc(size, /*enc_id=*/1, /*is_futex=*/is_futex, /*add_to_memlist=*/true)) == NULL) {
            log_err("Failed to allocate memory chunk.\n");
            return -ENOMEM;
        }
        vma->vm_pgoff = 0;  // reset pgoff
        /* Chuqi: 
         * Before remapping, call the secure monitor to claim
         * and protect the assigned physical memory. 
         * 
         * If the adversary refuse to claim this (i.e., hijack the 
         * control flow and bypass this security monitor call),
         * then SM will revoke the write access permission to the 
         * unclaimed enclave's internal memory during MMU update,
         * as the pages are not claimed here.
         */
        SM_encos_enclave_claim_memory(/*uva=*/vma->vm_start, /*pa=*/enc_mem->phys, 
                                      /*nr_pages=*/enc_mem->nr_pages, 
                                      /*enc_internal_mem=*/is_internalmem);
                                      
    }
    else {/* in case (2), mmap a shared memory */
        ENCOS_ASSERT(pg_off != 0, "Invalid pg_off=0x%lx.\n", pg_off);
        enc_id = pg_off;
        /* TODO: should also fetch the enc_id from the SM and check */
        ENCOS_ASSERT(enc_id == 1, "Invalid enc_id=0x%x.\n", enc_id);
        vma->vm_pgoff = 0;  // reset pgoff
        /* lookup first */
        shmem_entry = encos_shmem_lookup(enc_id);
        if (!shmem_entry) { /* lookup failed */
            /* assign with owner_pid (only if it has write permission) */
            if (!(vma->vm_flags & VM_WRITE)) {
                log_err("Shared memory owner does not have write permission.\n");
                return -EPERM;
            }
            /* allocate a shared memory chunk */
            if ((enc_mem = encos_shmem_alloc(size, /*enc_id=*/enc_id)) == NULL) {
                log_err("Failed to allocate shared memory chunk.\n");
                return -ENOMEM;
            }
        } else {   /* lookup succeeded */
            /* must have R/X but not W */
            if (vma->vm_flags & VM_WRITE) {
                log_err("Shared memory borrower cannot have write permission.\n");
                return -EPERM;
            }
            enc_mem = shmem_entry->shmem_chunk;
        }
    }

    /* start mmap */
    base_phys_addr = enc_mem->phys;
    phys_pfn = base_phys_addr >> PAGE_SHIFT;
    /* assign pages */
    pages = (struct page **)kmalloc(sizeof(struct page *) * enc_mem->nr_pages, 
                                    GFP_KERNEL);
    if (!pages) {
        log_err("Failed to allocate pages.\n");
        return -ENOMEM;
    }
    for (i = 0; i < enc_mem->nr_pages; i++) {
        page = pfn_to_page(phys_pfn + i);
        pages[i] = page;
    }
    num = enc_mem->nr_pages;

// #ifdef ENCOS_DEBUG
    // log_err("[try] vma: {vm_start=0x%lx(size: 0x%lx) => vm_pgoff=0x%lx} vm_flags=0x%lx, vm_page_prot=0x%lx.\n", 
    //          vma->vm_start, size, vma->vm_pgoff, vma->vm_flags, vma->vm_page_prot.pgprot);
// #endif

    // vm_flags_set(vma, VM_DONTEXPAND | VM_DONTDUMP | VM_READ | VM_WRITE | VM_SHARED);
    // if (remap_pfn_range(vma, start, pg_off, size, vma->vm_page_prot)) {
    //     log_err("Remap the physical memory failed.\n");
    //     return -EAGAIN;
    // }
    /* vm_insert_pages or remap_pfn_range? which one is better? */
    if ((rvl = vm_insert_pages(vma, start, pages, &num)) != 0) {
        log_err("Failed to insert all pages. nr_pages=%lu, failed: %lu, ret=%d.\n", 
                    enc_mem->nr_pages, num, rvl);
        // debug pages
        for (i = 0; i < enc_mem->nr_pages; i++) {
            log_err("page[%d]=0x%lx.\n", i, (unsigned long)pages[i]);
        }
        return -EAGAIN;
    }

    // /* remove, we have protected them once claimed (above) */
    // SM_encos_enclave_protect_memory(/*pa=*/enc_mem->phys, /*nr_pages=*/enc_mem->nr_pages);

// #ifdef ENCOS_DEBUG
//     log_err("[done] vma: {vm_start=0x%lx(size: 0x%lx) => vm_pgoff=0x%lx} vm_flags=0x%lx, vm_page_prot=0x%lx.\n", 
//              vma->vm_start, size, vma->vm_pgoff, vma->vm_flags, vma->vm_page_prot.pgprot);
// #endif
    kfree(pages);
    return 0;
}

static struct file_operations encos_dev_ops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = encos_ioctl,
    .mmap = encos_mmap,
};


static int __init encos_dev_init(void)
{
    int rvl;

    /* register device */
    misc = kzalloc(sizeof(struct miscdevice), GFP_KERNEL);
    misc->name = ENCOS_DEV_NAME;
    misc->minor = MISC_DYNAMIC_MINOR;
    /* 
     * Note: this mode (0666) is ONLY for development purpose.
     * Granting all users to access the device will increase
     * the risk of security.
     */
    misc->mode = 0666; // rw-rw-rw-
    misc->fops = &encos_dev_ops;

    rvl = misc_register(misc);
    if (rvl) {
        log_err("Failed to register misc device: %s\n", ENCOS_DEV_NAME);
        return rvl;
    }

    /* lock init */
    mutex_init(&encos_dev_mlock);
    /* allocator init */
    init_encos_allocator();

    /* finish */
    log_info("Initialized dev: %s (mode=%d).\n",
             ENCOS_DEV_NAME, misc->mode);

    /* perf_test */
    encos_micro_perf();
    return 0;
}

static void __exit encos_dev_exit(void)
{
    /* deregister device */
    misc_deregister(misc);
    kfree(misc);

    /* destroy allocator */
    destory_encos_allocator();

    log_info("Exit.\n");
}

module_init(encos_dev_init);
module_exit(encos_dev_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sun Devil");
MODULE_DESCRIPTION("ENCOS device driver");