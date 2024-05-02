#ifndef __ENCOS_ALLOC_H__
#define __ENCOS_ALLOC_H__
#include "common.h"

#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/sizes.h>
#include <linux/cma.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/sched.h>
#include <linux/hashtable.h>

#define ENCOS_CMA_SIZE  SZ_1G

extern struct cma *encos_cma;

typedef struct encos_mem {
    /* enclave id */
    int enc_id; 
    /* kernel virtual address */
    unsigned long virt_kern;
    /* user virutal address */
    unsigned long virt_user;
    /* underlining phys address */
    unsigned long phys;
    /* size */
    unsigned long length;
    unsigned long nr_pages;
    unsigned int cma_alloc : 1;
    /* link to the global chunks */
    struct list_head list;
} encos_mem_t;

extern struct list_head encos_mem_chunks;

typedef struct encos_shmem_hash_entry {
    /* key */
    int enc_id;
    /* shmem */
    int owner_pid;  // owner with write permission
    encos_mem_t *shmem_chunk;
    struct hlist_node hlist;
} encos_shmem_hash_entry_t;

// static u64 encos_shmem_hash_key(int encid)
// {
//     return hash_64(encid, 8);
// }

/**
 * Initialize the CMA allocator for the ENCOS driver.
 */
/* allocator */
    // if (!encos_cma) {
    //    cma_declare_contiguous(0, ENCOS_CMA_SIZE, 0, 0, 0, false, "encos", &encos_cma);
    // }
static inline void init_encos_allocator(void)
{
    /* memory chunk list */
    INIT_LIST_HEAD(&encos_mem_chunks);
}

/**
 * Destory the CMA allocator for the ENCOS driver.
 */
static inline void destory_encos_allocator(void)
{
    int nr_pages, order;
    struct page *page;

    /* free all chunks */
    encos_mem_t *pos, *n;
    list_for_each_entry_safe(pos, n, &encos_mem_chunks, list) {
        if (pos->cma_alloc) {
            page = virt_to_page((void *)pos->virt_kern);
            nr_pages = ALIGN(pos->length, PAGE_SIZE) >> PAGE_SHIFT;
            cma_release(NULL, page, nr_pages);
        } 
        else {
            order = get_order(pos->length);
            free_pages((unsigned long)pos->virt_kern, order);
        }
        list_del(&pos->list);
        kfree(pos);
    }
}

/**
 * Description
 */
static inline void encos_mem_inspect(encos_mem_t *mem)
{
    struct page *page;
    if (!mem) {
        log_err("NULL memory chunk.\n");
        return;
    }

    page = virt_to_page((void *)mem->virt_kern);
    log_info("MEM[enc_id=%d; cma=%d] phys=0x%lx, virt_kern=0x%lx, virt_user=0x%lx, length=%ld.\n",
             mem->enc_id, mem->cma_alloc, 
             mem->phys, mem->virt_kern, mem->virt_user, mem->length);
    log_err("MEM page=0x%lx, mapping=0x%lx.\n", 
             (unsigned long)page, (unsigned long)page->mapping);
}

/**
 * Allocate a memory chunk given a enclave id.
 */
encos_mem_t *encos_alloc(unsigned long length, unsigned long enc_id, bool add_to_memlist);


/**
 * Allocate a shared memory chunk given a enclave id.
 */
encos_mem_t *encos_shmem_alloc(unsigned long length, unsigned long enc_id);

encos_shmem_hash_entry_t *encos_shmem_lookup(unsigned long enc_id);
#endif

