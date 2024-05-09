#include "encos_alloc.h"
#include <linux/encos.h>
#include <linux/dma-map-ops.h>


struct cma *encos_cma = NULL;

struct list_head encos_mem_chunks;

struct hlist_head encos_shmem_table[];
DEFINE_HASHTABLE(encos_shmem_table, 8);

/**
 * Allocate a memory chunk.
 */
encos_mem_t *encos_alloc(unsigned long length, unsigned long enc_id, bool is_futex, bool add_to_memlist)
{
    struct page *page = NULL;
    int nr_pages, order;

    order = get_order(length);
    nr_pages = ALIGN(length, PAGE_SIZE) >> PAGE_SHIFT;

    encos_mem_t *encos_mem = (encos_mem_t *)
                                kzalloc(sizeof(encos_mem_t), GFP_KERNEL);
    if (!encos_mem)
        goto fail;

    encos_mem->enc_id = enc_id;
    encos_mem->owner_pid = current->pid;
    
    /*
     * If the LibOS is asking for allocating a futex, we can only give it an anonymous page.
     * Otherwise, `page->mapping` will be NULL and futex check won't pass.
     * Refer to kernel's get_futex_key() for more details.
     */
    if (is_futex) {
        // assert a futex (size 0x70 in our PAL) here
        ENCOS_ASSERT(length == 0x70, "Futex size is not 0x70.\n");
        encos_mem->virt_kern = (unsigned long)kmalloc(PAGE_SIZE, GFP_KERNEL);
        if (!encos_mem->virt_kern) {
            kfree(encos_mem);
            goto fail;
        }
        encos_mem->phys = (unsigned long)virt_to_phys((void *)encos_mem->virt_kern);
        encos_mem->length = PAGE_SIZE;
        encos_mem->nr_pages = 1;
        encos_mem->alloc_type = 2;
        /* PAGE ALIGNED */
        ENCOS_ASSERT((encos_mem->virt_kern & 0xFFF) == 0, 
            "Futex KVA is not page aligned.\n");
        goto succ;
    }
    /*
	 * For anything below order 1 allocations rely on the buddy
	 * allocator. If such low-order allocations can't be handled
	 * anymore the system won't work anyway.
	 */
    /* CMA allocator */
	if (order > 0) {
        page = dma_alloc_from_contiguous(NULL, nr_pages, 1, false);
        if (page) {
            encos_mem->virt_kern = (unsigned long)page_to_virt(page);
            encos_mem->phys = (unsigned long)page_to_phys(page);
            encos_mem->length = length;
            encos_mem->alloc_type = 1;
            encos_mem->nr_pages = nr_pages;

            // debug assert
            ENCOS_ASSERT((page + 1) == virt_to_page((void *)encos_mem->virt_kern + PAGE_SIZE), 
                "page+1=0x%lx, virt_to_page=0x%lx.\n", 
                 (unsigned long)(page+1), 
                 (unsigned long)virt_to_page((void *)encos_mem->virt_kern + PAGE_SIZE));
            goto succ;
        } else {
            log_err("Failed to allocate memory (length=0x%lx, nr_pages=%d, order=%d) from CMA.\n",
                        length, nr_pages, order);
            goto fail;
        }
    }
    /* buddy allocator */
    encos_mem->virt_kern = (unsigned long)__get_free_pages(
                                GFP_KERNEL | __GFP_RETRY_MAYFAIL, order);
    if (!encos_mem->virt_kern) {
        kfree(encos_mem);
        goto fail;
    }
    encos_mem->phys = (unsigned long)virt_to_phys((void *)encos_mem->virt_kern);
    encos_mem->length = length;
    encos_mem->alloc_type = 0;
    encos_mem->nr_pages = nr_pages;
succ:
    /* 
     * For enclave internal memory, we add them to the list.
     * For shared memory, we don't do so.
     */
    if (add_to_memlist)
        list_add_tail(&encos_mem->list, &encos_mem_chunks);
    /* clear content */
    memset((void *)encos_mem->virt_kern, 0, length);
// #ifdef ENCOS_DEBUG
//     // /* inspect the allocated memory */
//     log_info("[enc_id=%d,pid=%d] Allocated memory chunk (order=%d, nr_page=%lu): \n", 
//             (int)enc_id, current->pid, order, encos_mem->nr_pages);
//     encos_mem_inspect(encos_mem);
// #endif 
    return encos_mem;

fail:
    log_err("Failed to allocate memory size=0x%lx, enc_id=%lu.\n",
            length, enc_id);
    return NULL;
}

void encos_free(encos_mem_t *encos_mem)
{
    struct page *page;
    int order, i;
    unsigned long count = 0;

    if (!encos_mem) {
        log_err("NULL memory chunk.\n");
        return;
    }
    /* check page count */
    for (i = 0; i < encos_mem->nr_pages; i++)
    {
        page = pfn_to_page((encos_mem->phys >> PAGE_SHIFT) + i);
        count += (page_count(page) != 1);
    }
    /* still in use. let's ignore this chunk */
    if (count != 0) {
#ifdef ENCOS_DEBUG
        log_info("[enc=%d,pid=%d] Ignore free chunk KVA=0x%lx, PA=0x%lx (length=0x%lx), pagecount=%lu.\n",
            encos_mem->enc_id, current->pid, 
            encos_mem->virt_kern, encos_mem->phys, encos_mem->length,
            count);
#endif
        return;
    }
    if (encos_mem->alloc_type == 1) {
        page = virt_to_page((void *)encos_mem->virt_kern);
        dma_release_from_contiguous(NULL, page, encos_mem->nr_pages);
    } else if (encos_mem->alloc_type == 0) {
        order = get_order(encos_mem->length);
        free_pages(encos_mem->virt_kern, order);
    } else {
        kfree((void *)encos_mem->virt_kern);
    }
#ifdef ENCOS_DEBUG
    log_info("[enc=%d,pid=%d] Done free chunk KVA=0x%lx, PA=0x%lx (length=0x%lx).\n",
            encos_mem->enc_id, current->pid, 
            encos_mem->virt_kern, encos_mem->phys, encos_mem->length);
#endif
}

void encos_enclave_free_all(int enc_id, int owner_pid)
{
    struct list_head *pos, *q;
    struct list_head *head;
    encos_mem_t *tmp;

    head = &encos_mem_chunks;
    list_for_each_safe(pos, q, head) {
        tmp = list_entry(pos, encos_mem_t, list);
        if (tmp->enc_id == enc_id && tmp->owner_pid == owner_pid) { /* release */
            list_del(pos);
            encos_free(tmp);
            kfree(tmp);
        }
    }
}
EXPORT_SYMBOL(encos_enclave_free_all);

encos_mem_t *encos_shmem_alloc(unsigned long length, unsigned long enc_id)
{
    /* only shmem owner can allocate id */
    encos_mem_t *shmem_chunk;
    encos_shmem_hash_entry_t *shmem_entry;
    int owner_pid = current->pid;

    /* do an untrusted entry free. bad code. useless :( */
    free_enclave_ut(owner_pid);

    shmem_chunk = encos_alloc(length, /*enc_id=*/enc_id, /*is_futex=*/false,
                              /*add_to_memlist=*/false);
    if (!shmem_chunk) {
        log_err("Failed to allocate shared memory chunk.\n");
        return NULL;
    }

#ifdef ENCOS_DEBUG
    log_info("Allocated shmem chunk succeed for owner_pid=%d, enc_id=%lu.\n",
              owner_pid, enc_id);
#endif 

    /* insert the entry to hash table */
    shmem_entry = (encos_shmem_hash_entry_t *)
                    kzalloc(sizeof(encos_shmem_hash_entry_t), GFP_KERNEL | __GFP_ZERO);
    shmem_entry->enc_id = enc_id;
    shmem_entry->owner_pid = owner_pid;
    shmem_entry->shmem_chunk = shmem_chunk;
    
    hash_add(encos_shmem_table, &shmem_entry->hlist, enc_id);
    return shmem_chunk;
}

encos_shmem_hash_entry_t *encos_shmem_lookup(unsigned long enc_id)
{
    encos_shmem_hash_entry_t *shmem_entry;
    hash_for_each_possible(encos_shmem_table, shmem_entry, hlist, enc_id) {
        if (shmem_entry->enc_id == enc_id)
            return shmem_entry;
    }
    
    /* no such hash */
    log_err("Failed to find shared memory chunk with enc_id=%lu.\n", enc_id);
    return NULL;
}

// void encos_shmem_table_destroy(void)
// {
//     unsigned bkt;
//     encos_shmem_hash_entry_t *shmem_entry;
//     struct hlist_node *tmp;

//     /* iterate all table */
//     hash_for_each_safe(encos_shmem_table, bkt, tmp, shmem_entry, hlist) {
//         hash_del(&shmem_entry->hlist);
//         if (shmem_entry->shmem_chunk) {
            
//         }
//         kfree(shmem_entry);
//     }
//     log_info("[Done] Destroyed encos shared memory hash table.\n");
// }