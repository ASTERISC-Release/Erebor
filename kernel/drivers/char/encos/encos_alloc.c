#include "encos_alloc.h"

struct cma *encos_cma = NULL;

struct list_head encos_mem_chunks;

/**
 * Allocate a memory chunk.
 */
encos_mem_t *encos_alloc(unsigned long length, unsigned long enc_id)
{
    struct page *page = NULL;
    int nr_pages, order;

    order = get_order(length);
    nr_pages = ALIGN(length, PAGE_SIZE) >> PAGE_SHIFT;

    encos_mem_t *encos_mem = (encos_mem_t *)
                                kzalloc(sizeof(encos_mem_t), GFP_KERNEL);
    if (!encos_mem)
        return NULL;
    
    encos_mem->enc_id = enc_id;
    /*
	 * For anything below order 3 allocations rely on the buddy
	 * allocator. If such low-order allocations can't be handled
	 * anymore the system won't work anyway.
	 */
	if (order > 2)
		page = cma_alloc(encos_cma, nr_pages, 0, false);
	if (page) {
        encos_mem->virt_kern = (unsigned long)page_to_virt(page);
        encos_mem->phys = (unsigned long)page_to_phys(page);
        encos_mem->length = length;
        encos_mem->cma_alloc = 1;
        goto succ;
	}
    /* buddy allocator */
    encos_mem->virt_kern = (unsigned long)__get_free_pages(
                                GFP_KERNEL | __GFP_RETRY_MAYFAIL, order);
    if (!encos_mem->virt_kern) {
        kfree(encos_mem);
        return NULL;
    }
    encos_mem->phys = (unsigned long)virt_to_phys((void *)encos_mem->virt_kern);
    encos_mem->length = length;
    encos_mem->cma_alloc = 0;
succ:
    list_add_tail(&encos_mem->list, &encos_mem_chunks);
    /* clear content */
    memset((void *)encos_mem->virt_kern, 0, length);
    return encos_mem;
}