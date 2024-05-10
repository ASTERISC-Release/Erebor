/*===- mmu.c - SVA Execution Engine  =-------------------------------------===
 * 
 *                        Secure Virtual Architecture
 *
 * This file was developed by the LLVM research group and is distributed under
 * the University of Illinois Open Source License. See LICENSE.TXT for details.
 * 
 *===----------------------------------------------------------------------===
 *
 * Note: We try to use the term "frame" to refer to a page of physical memory
 *       and a "page" to refer to the virtual addresses mapped to the page of
 *       physical memory.
 *
 *===----------------------------------------------------------------------===
 */

#include <linux/types.h>

#include "sva/config.h"
#include "sva/mmu.h"
#include "sva/mmu_intrinsics.h"
#include "sva/stack.h"
#include "sva/svamem.h"
#include "sva/x86.h"
#include "sva/pks.h"
#include "sva/idt.h"

#define NCPU       24

/* 
 * Description: 
 *   This is a pointer to the PerspicuOS SuperSpace stack, which is used on
 *   calls to SuperSpace or SuperSpace calls.
 */
char SecureStack[4096*NCPU] SVAMEM;
// TODO: Important this value can't be changed from outside the nested kernel!
const uintptr_t SecureStackBase = (uintptr_t) SecureStack + 4096;


#undef NKDEBUGG
#define NKDEBUGG(fname, fmt, args...) /* nothing: it's a placeholder */


/*
 *****************************************************************************
 * MSR and control register (CR) operations.
 *****************************************************************************
 */
// Rahul: Moved functions here from the header file mmu.h
 uint64_t
_rdmsr(uintptr_t msr)
{
    uint32_t low, high;

    __asm __volatile("rdmsr" : "=a" (low), "=d" (high) : "c" (msr));
    return (low | ((uint64_t)high << 32));
}

void
_load_cr0(unsigned long val) {
    __asm __volatile("movq %0,%%cr0" : : "r" (val));
}

void
_load_cr4(unsigned long val) {
    __asm __volatile("movq %0,%%cr4" : : "r" (val));
}

void
_wrmsr(uintptr_t msr, uint64_t newval)
{
	uint32_t low, high;

	low = newval;
	high = newval >> 32;
	__asm __volatile("wrmsr" : : "a" (low), "d" (high), "c" (msr));
}

 void _load_cr3(unsigned long data)
{ 
    __asm __volatile("movq %0,%%cr3" : : "r" (data) : "memory"); 
}

 uintptr_t
_rcr0(void) {
    uintptr_t  data;
    __asm __volatile("movq %%cr0,%0" : "=r" (data));
    return (data);
}

 uintptr_t
_rcr3(void) {
    uintptr_t  data;
    __asm __volatile("movq %%cr3,%0" : "=r" (data));
    return (data);
}

 uintptr_t
_rcr4(void) {
    uintptr_t  data;
    __asm __volatile("movq %%cr4,%0" : "=r" (data));
    return (data);
}

 uint64_t
_efer(void) {
    return _rdmsr(MSR_REG_EFER);
}

/*
 *****************************************************************************
 * Function prototype declarations.
 *****************************************************************************
 */

/*
 * Private local mapping update function prototypes.
 */
static  void __update_mapping (uintptr_t * pageEntryPtr, page_entry_t val);

/*
 *****************************************************************************
 * Define paging structures and related constants local to this source file
 *****************************************************************************
 */

/* Flags whether the MMU has been initialized */
static unsigned char mmuIsInitialized = 0;


/* Array describing the physical pages */
/* The index is the physical page number */
static page_desc_t page_desc[numPageDescEntries] SVAMEM;



/*
 * Object: MMULock
 *
 * Description:
 *  This is the spinlock used for synchronizing access to the page tables.
 *  Chuqi: TODO: replace the normal OS's spinlock with our own spinlock.
 */
spinlock_t MMULock;

static void init_MMULock(void) {
    spin_lock_init(&MMULock);
}

static void MMULock_Acquire(void) {
    spin_lock(&MMULock);
}

static void MMULock_Release(void) {
    spin_unlock(&MMULock);
}

/*
 * Description:
 *  Given a page table entry value, return the page description associate with
 *  the frame being addressed in the mapping.
 *
 * Inputs:
 *  mapping: the mapping with the physical address of the referenced frame
 *
 * Return:
 *  Pointer to the page_desc for this frame
 */
page_desc_t * getPageDescPtr(unsigned long mapping) {
  unsigned long frameIndex = (mapping & PG_FRAME) / pageSize;

  if(frameIndex  >= numPageDescEntries)
    panic ("[PANIC]: SVA: getPageDescPtr: %lx %lx %lx\n", mapping, frameIndex, numPageDescEntries);
  return page_desc + frameIndex;
}

/*
 * Function: init_mmu
 *
 * Description:
 *  Initialize MMU data structures.
 */
void 
init_mmu () {
  /* Initialize the page descriptor array */
  memset (page_desc, 0, sizeof (struct page_desc_t) * numPageDescEntries);

  return;
}

/*
 *****************************************************************************
 * Define helper functions for MMU operations
 *****************************************************************************
 */

/* Functions for aiding in declare and updating of page tables */

/*
 * Function: page_entry_store
 *
 * Description:
 *  This function takes a pointer to a page table entry and updates its value
 *  to the new value provided.
 *
 * Assumptions: 
 *  - This function assumes that write protection is enabled in CR0 (WP bit set
 *    to 1). 
 *
 * Inputs:
 *  *page_entry -: A pointer to the page entry to store the new value to, a
 *                 valid VA for accessing the page_entry.
 *  newVal      -: The new value to store, including the address of the
 *                 referenced page.
 *
 * Side Effect:
 *  - This function enables system wide write protection in CR0. 
 *    
 *
 */
static  void
page_entry_store (unsigned long *page_entry, page_entry_t newVal) {
  /* Write the new value to the page_entry */
  *page_entry = newVal;

  /* TODO: Add a check here to make sure the value matches the one passed in */
}

/*
 *****************************************************************************
 * Page table page index and entry lookups 
 *****************************************************************************
 */

/*
 * Function: pt_update_is_valid()
 *
 * Description:
 *  This function assesses a potential page table update for a valid mapping.
 *
 *  NOTE: This function assumes that the page being mapped in has already been
 *  declared and has its intial page metadata captured as defined in the
 *  initial mapping of the page.
 *
 * Inputs:
 *  *page_entry  - VA pointer to the page entry being modified
 *  newVal       - Representes the new value to write including the reference
 *                 to the underlying mapping.
 *
 * Return:
 *  0  - The update is not valid and should not be performed.
 *  1  - The update is valid but should disable write access.
 *  2  - The update is valid and can be performed.
 */
static unsigned char
pt_update_is_valid (page_entry_t *page_entry, page_entry_t newVal) {
  /* Collect associated information for the existing mapping */
  unsigned long origPA = *page_entry & PG_FRAME;
  unsigned long origFrame = origPA >> PAGESHIFT;
  uintptr_t origVA = (uintptr_t) getVirtual(origPA);

  page_desc_t *origPG = getPageDescPtr(origPA);

  /* Get associated information for the new page being mapped */
  unsigned long newPA = newVal & PG_FRAME;
  unsigned long newFrame = newPA >> PAGESHIFT;
  uintptr_t newVA = (uintptr_t) getVirtual(newPA);
  page_desc_t *newPG = getPageDescPtr(newPA);

  /* Get the page table page descriptor. The page_entry is the viratu */
  uintptr_t ptePAddr = __pa (page_entry);
  page_desc_t *ptePG = getPageDescPtr(ptePAddr);


  /* Return value */
  unsigned char retValue = 2;

  /* 
   * If we aren't mapping a new page then we can skip several checks, and in
   * some cases we must, otherwise, the checks will fail. For example if this
   * is a mapping in a page table page then we allow a zero mapping. 
   */
  // if (newVal & PG_V) {
    /* If the mapping is to an SVA page then fail */
    SVA_ASSERT (!isSVAPg(newPG), "Kernel attempted to map an SVA page");

    /*
     * New mappings to code pages are permitted as long as they are either
     * for user-space pages or do not permit write access.
     */
    if (isCodePg (newPG)) {
      if ((newVal & (PG_RW | PG_U)) == (PG_RW)) {
        panic ("SVA: Making kernel code writeable: %lx %lx\n", newVA, newVal);
      }
    }

    /* 
     * If the new page is a page table page, then we verify some page table
     * page specific checks. 
     */
    if (isPTP(newPG)) {
      /* 
       * If we have a page table page being mapped in and it currently
       * has a mapping to it, then we verify that the new VA from the new
       * mapping matches the existing currently mapped VA.   
       *
       * This guarantees that we each page table page (and the translations
       * within it) maps a singular region of the address space.
       *
       * Otherwise, this is the first mapping of the page, and we should record
       * in what virtual address it is being placed.
       */
#if 0
      if (pgRefCount(newPG) > 1) {
        if (newPG->pgVaddr != page_entry) {
          panic ("SVA: PG: %lx %lx: type=%x\n", newPG->pgVaddr, page_entry, newPG->type);
        }
        SVA_ASSERT (newPG->pgVaddr == page_entry, "MMU: Map PTP to second VA");
      } else {
        newPG->pgVaddr = page_entry;
      }
#endif
    }

    /*
     * Verify that that the mapping matches the correct type of page
     * allowed to be mapped into this page table. Verify that the new
     * PTP is of the correct type given the page level of the page
     * entry. 
     */
    switch (ptePG->type) {
      case PG_L1:
        if (!isFramePg(newPG)) {
          /*
           * If it is a page table page, just ensure that it is not writeable.
           * The kernel may be modifying the direct map, and we will permit
           * that as long as it doesn't make page tables writeable.
           *
           * Note: The SVA VM really should have its own direct map that the
           *       kernel cannot use or modify, but that is too much work, so
           *       we make this compromise.
           */
          if ((newPG->type >= PG_L1) && (newPG->type <= PG_L5)) {
            retValue = 2;
          } else {
            panic ("SVA: MMU: Map bad page type into L1: %x\n", newPG->type);
          }
        }

        break;

      case PG_L2:
        if (newVal & PG_PS) {
          if (!isFramePg(newPG)) {
            /*
             * If it is a page table page, just ensure that it is not writeable.
             * The kernel may be modifying the direct map, and we will permit
             * that as long as it doesn't make page tables writeable.
             *
             * Note: The SVA VM really should have its own direct map that the
             *       kernel cannot use or modify, but that is too much work, so
             *       we make this compromise.
             */
            if ((newPG->type >= PG_L1) && (newPG->type <= PG_L5)) {
              retValue = 2;
            } else {
              panic ("SVA: MMU: Map bad page type into L2: %x\n", newPG->type);
            }
          }
        } else {
          SVA_ASSERT (isL1Pg(newPG), "MMU: Mapping non-L1 page into L2.");
        }
        break;

      case PG_L3:
        if (newVal & PG_PS) {
          if (!isFramePg(newPG)) {
            /*
             * If it is a page table page, just ensure that it is not writeable.
             * The kernel may be modifying the direct map, and we will permit
             * that as long as it doesn't make page tables writeable.
             *
             * Note: The SVA VM really should have its own direct map that the
             *       kernel cannot use or modify, but that is too much work, so
             *       we make this compromise.
             */
            if ((newPG->type >= PG_L1) && (newPG->type <= PG_L5)) {
              retValue = 2;
            } else {
              panic ("SVA: MMU: Map bad page type into L2: %x\n", newPG->type);
            }
          }
        } else {
          SVA_ASSERT (isL2Pg(newPG), "MMU: Mapping non-L2 page into L3.");
        }
        break;

      case PG_L4:
        SVA_ASSERT (isL3Pg(newPG), 
                    "MMU: Mapping non-L3 page into L4.");
        break;

      case PG_L5:
        SVA_ASSERT (isL4Pg(newPG), 
                    "MMU: Mapping non-L4 page into L5.");
        break;

      default:
        break;
    }
  // }

  /*
   * If the new mapping is set for user access, but the VA being used is to
   * kernel space, fail. Also capture in this check is if the new mapping is
   * set for super user access, but the VA being used is to user space, fail.
   *
   * 3 things to assess for matches: 
   *  - U/S Flag of new mapping
   *  - Type of the new mapping frame
   *  - Type of the PTE frame
   * 
   * Ensures the new mapping U/S flag matches the PT page frame type and the
   * mapped in frame's page type, as well as no mapping kernel code pages
   * into userspace.
   */
  
  /* 
   * If the original PA is not equivalent to the new PA then we are creating
   * an entirely new mapping, thus make sure that this is a valid new page
   * reference. Also verify that the reference counts to the old page are
   * sane, i.e., there is at least a current count of 1 to it. 
   */
  if (origPA != newPA) {
    /* 
     * If the old mapping was to a code page then we know we shouldn't be
     * pointing this entry to another code page, thus fail.
     */
    if (isCodePg (origPG)) {
      SVA_ASSERT ((*page_entry & PG_U),
                  "Kernel attempting to modify code page mapping");
    }
  }

  return retValue;
}

/*
 * Function: updateNewPageData
 *
 * Description: 
 *  This function is called whenever we are inserting a new mapping into a page
 *  entry. The goal is to manage any SVA page data that needs to be set for
 *  tracking the new mapping with the existing page data. This is essential to
 *  enable the MMU verification checks.
 *
 * Inputs:
 *  mapping - The new mapping to be inserted in x86_64 page table format.
 */
static  void
updateNewPageData(page_entry_t mapping) {
  uintptr_t newPA = mapping & PG_FRAME;
  page_desc_t *newPG = getPageDescPtr(mapping);

  /*
   * If the new mapping is valid, update the counts for it.
   */
  if (mapping & PG_V) {
#if 0
    /*
     * If the new page is to a page table page and this is the first reference
     * to the page, we need to set the VA mapping this page so that the
     * verification routine can enforce that this page is only mapped
     * to a single VA. Note that if we have gotten here, we know that
     * we currently do not have a mapping to this page already, which
     * means this is the first mapping to the page. 
     */
    if (isPTP(newPG)) {
      newPG->pgVaddr = newVA;
    }
#endif

    /* 
     * Update the reference count for the new page frame. Check that we aren't
     * overflowing the counter.
     */
    SVA_ASSERT (pgRefCount(newPG) < ((1u << 13) - 1), 
                "MMU: overflow for the mapping count");
    newPG->count++;

    /* 
     * Set the VA of this entry if it is the first mapping to a page
     * table page.
     */
  }

  return;
}

/*
 * Function: updateOrigPageData
 *
 * Description:
 *  This function updates the metadata for a page that is being removed from
 *  the mapping. 
 * 
 * Inputs:
 *  mapping - An x86_64 page table entry describing the old mapping of the page
 */
static  void
updateOrigPageData(page_entry_t mapping) {
  page_desc_t *origPG = getPageDescPtr(mapping);

  /* 
   * Only decrement the mapping count if the page has an existing valid
   * mapping.  Ensure that we don't drop the reference count below zero.
   */
  if ((mapping & PG_V) && (origPG->count)) {
    --(origPG->count);
  }

  return;
}

/*
 * Function: __do_mmu_update
 *
 * Description:
 *  If the update has been validated, this function manages metadata by
 *  updating the internal SVA reference counts for pages and then performs the
 *  actual update. 
 *
 * Inputs: 
 *  *page_entry  - VA pointer to the page entry being modified 
 *  newVal       - Representes the mapping to insert into the page_entry
 */
static  void
__do_mmu_update (page_entry_t* pteptr, page_entry_t mapping) {
  uintptr_t origPA = (uintptr_t)(*pteptr & PG_FRAME);
  uintptr_t newPA = (uintptr_t)(mapping & PG_FRAME);

  /*
   * If we have a new mapping as opposed to just changing the flags of an
   * existing mapping, then update the SVA meta data for the pages. We know
   * that we have passed the validation checks so these updates have been
   * vetted.
   */
  if (newPA != origPA) {
    if(*pteptr & PG_V) updateOrigPageData(origPA);
    if(mapping & PG_V) updateNewPageData(newPA);
  } else if ((*pteptr & PG_V) && ((mapping & PG_V) == 0)) {
    /*
     * If the old mapping is marked valid but the new mapping is not, then
     * decrement the reference count of the old page.
     */
    updateOrigPageData(origPA);
  } else if (((*pteptr & PG_V) == 0) && (mapping & PG_V)) {
    /*
     * Contrariwise, if the old mapping is invalid but the new mapping is valid,
     * then increment the reference count of the new page.
     */
    updateNewPageData(newPA);
  }

  /* Perform the actual write to into the page table entry */
  page_entry_store ((page_entry_t *)pteptr, mapping);
  return;
}

/*
 * Function: initDeclaredPage
 *
 * Description:
 *  This function zeros out the physical page pointed to by frameAddr and
 *  changes the permissions of the page in the direct map to read-only.
 *  This function is agnostic as to which level page table entry we are
 *  modifying because the format of the entry is the same in all cases. 
 *
 * Assumption: This function should only be called by a declare intrinsic.
 *      Otherwise it has side effects that may break the system.
 *
 * Inputs:
 *  frameAddr: represents the physical address of this frame
 *
 *  *page_entry: A pointer to a page table entry that will be used to
 *      initialize the mapping to this newly created page as read only. Note
 *      that the address of the page_entry must be a virtually accessible
 *      address.
 */
static  void 
initDeclaredPage (unsigned long frameAddr) {
  return;
  /*
   * Get the direct map virtual address of the physical address.
   */
  unsigned char * vaddr =  __va(frameAddr);

  /*
   * Initialize the contents of the page to zero.  This will ensure that no
   * existing page translations which have not been vetted exist within the
   * page.
   */
  memset (vaddr, 0, X86_PAGE_SIZE);

  /*
   * Get a pointer to the page table entry that maps the physical page into the
   * direct map.
   */
  page_entry_t * page_entry = get_pgeVaddr (vaddr);
  if (page_entry) {
    /*
     * Make the direct map entry for the page read-only to ensure that the OS
     * goes through SVA to make page table changes.  Also be sure to flush the
     * TLBs for the direct map address to ensure that it's made read-only
     * right away.
     */
    if (((*page_entry) & PG_PS) == 0) {
      // CLEANUP: Need to set to read-only anymore, since we use PKS now
      page_entry_store (page_entry, setMappingReadWrite(*page_entry)); // Rahul: Change to read-only once done testing
      sva_mm_flush_tlb (vaddr);
    }
  }

  return;
}

/*
 * Function: __update_mapping
 *
 * Description:
 *  Mapping update function that is agnostic to the level of page table. Most
 *  of the verification code is consistent regardless of which level page
 *  update we are doing. 
 *
 * Inputs:
 *  - pageEntryPtr : reference to the page table entry to insert the mapping
 *      into
 *  - val : new entry value
 */
static void
__update_mapping (uintptr_t * pageEntryPtr, page_entry_t val) {
  /* 
   * If the given page update is valid then store the new value to the page
   * table entry, else raise an error.
   */
  switch (pt_update_is_valid((page_entry_t *) pageEntryPtr, val)) {
    case 1:
      // Kernel thinks these should be RW, since it wants to write to them.
      // Convert to read-only and carry on.
      // CLEANUP: Need to set to read-only anymore, since we use PKS now
      val = setMappingReadOnly (val);
      __do_mmu_update ((page_entry_t *) pageEntryPtr, val);
      break;

    case 2:
      __do_mmu_update ((page_entry_t *) pageEntryPtr, val);
      break;

    case 0:
      /* Silently ignore the request */
      panic("Invalid mmu update requested!\n");
      return;

    default:
      panic("##### SVA invalid page update!!!\n");
  }

  return;
}

/* Functions for finding the virtual address of page table components */

/* 
 * Function: get_pgeVaddr
 *
 * Description:
 *  This function does page walk to find the entry controlling access to the
 *  specified address. The function takes into consideration the potential use
 *  of larger page sizes.
 * 
 * Inputs:
 *  vaddr - Virtual Address to find entry for
 *
 * Return value:
 *  0 - There is no mapping for this virtual address.
 *  Otherwise, a pointer to the PTE that controls the mapping of this virtual
 *  address is returned.
 */
page_entry_t * 
get_pgeVaddr (uintptr_t vaddr) {
  /* Pointer to the page table entry for the virtual address */
  page_entry_t *pge = 0;

  /* Get the base of the pml4 to traverse */
  uintptr_t cr3 = (uintptr_t) get_pagetable();
  if ((cr3 & 0xfffffffffffff000u) == 0)
    return 0;

  /* Get the VA of the pml4e for this vaddr */
  pgd_t *pgd = get_pgdVaddr ((unsigned char *)cr3, vaddr);

  if (pgd->pgd & PG_V) {
    p4d_t *p4d = get_p4dVaddr(pgd, vaddr);

    if(p4d->p4d & PG_V) {
      /* Get the VA of the pdpte for this vaddr */
      pud_t *pud = get_pudVaddr (p4d, vaddr);

      if (pud->pud & PG_V) {
        /* 
        * The PDPE can be configurd in large page mode. If it is then we have the
        * entry corresponding to the given vaddr If not then we go deeper in the
        * page walk.
        */
        if (pud->pud & PG_PS) {
          pge = (page_entry_t*)&pud->pud;
        } else {
          /* Get the pde associated with this vaddr */
          pmd_t *pmd = get_pmdVaddr (pud, vaddr);

          if (pmd->pmd & PG_V) {
            /* 
            * As is the case with the pdpte, if the pde is configured for large
            * page size then we have the corresponding entry. Otherwise we need
            * to traverse one more level, which is the last. 
            */
            if (pmd->pmd & PG_PS) {
              pge = (page_entry_t*)&pmd->pmd;
            } else {
              pte_t *pte = get_pteVaddr (pmd, vaddr);
              pge = (page_entry_t*)&pte->pte;
            }
          }
        }
      }
    }
  }

  /* Return the entry corresponding to this vaddr */
  return pge;
}

/* Refer to Intel SDM Section 4.5.2 */
pgd_t *
get_pgdVaddr (unsigned char * cr3, uintptr_t vaddr) {
  /* Offset into the page table */
  uintptr_t offset = (vaddr >> (48 - 3)) & vmask;
  return (pgd_t *) getVirtual (((uintptr_t)cr3) | offset);
}

p4d_t *
get_p4dVaddr (pgd_t * pgd, uintptr_t vaddr) {
  /* Offset into the page table */
  uintptr_t base   = (uintptr_t)(pgd->pgd) & addrmask;
  uintptr_t offset = (vaddr >> (39 - 3)) & vmask;
  return (p4d_t *) getVirtual (((uintptr_t)base) | offset);
}

pud_t *
get_pudVaddr (p4d_t * p4d, uintptr_t vaddr) {
  uintptr_t base   = (uintptr_t)(p4d->p4d) & addrmask;
  uintptr_t offset = (vaddr >> (30 - 3)) & vmask;
  return (pud_t *) getVirtual (base | offset);
}

pmd_t *
get_pmdVaddr (pud_t * pud, uintptr_t vaddr) {
  uintptr_t base   = (uintptr_t)(pud->pud) & addrmask;
  uintptr_t offset = (vaddr >> (21 - 3)) & vmask;
  return (pmd_t *) getVirtual (base | offset);
}

pte_t *
get_pteVaddr (pmd_t * pmd, uintptr_t vaddr) {
  uintptr_t base   = (uintptr_t)(pmd->pmd) & addrmask;
  uintptr_t offset = (vaddr >> (12 - 3)) & vmask;
  return (pte_t *) getVirtual (base | offset);
}


/*
 * Functions for returing the physical address of page table pages.
 */
static  uintptr_t
get_pgdPaddr (unsigned char * cr3, uintptr_t vaddr) {
  /* Offset into the page table */
  uintptr_t offset = ((vaddr >> 48) << 3) & vmask;
  return (((uintptr_t)cr3) | offset);
}

static  uintptr_t
get_p4dPaddr (pgd_t * pgd, uintptr_t vaddr) {
  uintptr_t offset = ((vaddr  >> 39) << 3) & vmask;
  return (((uintptr_t)(pgd->pgd) & 0x000ffffffffff000u) | offset);
}

static  uintptr_t
get_pudPaddr (p4d_t * p4d, uintptr_t vaddr) {
  uintptr_t offset = ((vaddr  >> 30) << 3) & vmask;
  return (((uintptr_t)p4d->p4d & 0x000ffffffffff000u) | offset);
}

static  uintptr_t
get_pmdPaddr (pud_t * pud, uintptr_t vaddr) {
  uintptr_t offset = ((vaddr >> 21) << 3) & vmask;
  return (((uintptr_t)pud->pud & 0x000ffffffffff000u) | offset);
}

static  uintptr_t
get_ptePaddr (pmd_t * pmd, uintptr_t vaddr) {
  uintptr_t offset = ((vaddr >> 12) << 3) & vmask;
  return (((uintptr_t)pmd->pmd & 0x000ffffffffff000u) | offset);
}

/* Functions for querying information about a page table entry */
// Rahul: Check instances that invoke this function and if the argument passing is correct
static  unsigned char
isPresent (uintptr_t * pte) {
  return (*pte & 0x1u) ? 1u : 0u;
}

/*
 * Function: getPhysicalAddr()
 *
 * Description:
 *  Find the physical page number of the specified virtual address.
 */
uintptr_t
getPhysicalAddr (void * v) {
  /* Mask to get the proper number of bits from the virtual address */
  static const uintptr_t vmask = 0x0000000000000fffu;

  /* Virtual address to convert */
  uintptr_t vaddr  = ((uintptr_t) v);

  /* Offset into the page table */
  uintptr_t offset = 0;

  /*
   * Get the currently active page table.
   */
  unsigned char * cr3 = get_pagetable();

  /*
   * Get the address of the PGD.
   */
  pgd_t * pgd = get_pgdVaddr (cr3, vaddr);

  /*
   * Use the PGD to get the address of the P4D.
   */
  p4d_t * p4d = get_p4dVaddr (pgd, vaddr);

  /*
   * Use the P4D to get the address of the PUD.
   */
  pud_t * pud = get_pudVaddr (p4d, vaddr);

  /*
   * Determine if the PUD has the PS flag set.  If so, then it's pointing to
   * a 1 GB page; return the physical address of that page.
   */
  if (pud->pud & PTE_PS) {
    return (pud->pud & 0x000fffffffffffffu) >> 30;
  }

  /*
   * Find the PMD entry table from the PUD value.
   */
  pmd_t * pmd = get_pmdVaddr (pud, vaddr);

  /*
   * Determine if the PMD has the PS flag set.  If so, then it's pointing to a
   * 2 MB page; return the physical address of that page.
   */
  if (pmd->pmd & PTE_PS) {
    return (pmd->pmd & 0x000fffffffe00000u) + (vaddr & 0x1fffffu);
  }

  /*
   * Find the PTE pointed to by this PDE.
   */
  pte_t * pte = get_pteVaddr (pmd, vaddr);

  /*
   * Compute the physical address.
   */
  offset = vaddr & vmask;
  uintptr_t paddr = (pte->pte & 0x000ffffffffff000u) + offset;
  return paddr;
}


SECURE_WRAPPER(void, 
sva_mmu_test, void) {
  printk("sva_mmu_test\n");
}

/*
 * Intrinsic: sva_mm_load_pgtable()
 *
 * Description:
 *  Set the current page table.  This implementation will also enable paging.
 *
 *  To protect against direct abuse by the outer kernel, we leave the actual
 *  instruction to write to cr3 unmapped. Then when called the function
 *  disables paging, inserts the mapping to the code page for cr3, jumps to
 *  execution there, and upon return removes the mapping and flushes the TLB.
 *
 * Inputs:
 *  pg - The physical address of the top-level page table page.
 */
SECURE_WRAPPER(void,
sva_mm_load_pgtable, void *pg) {
  MMULock_Acquire();
  /* Control Register 0 Value (which is used to enable paging) */
  unsigned int cr0;

  /*
   * TODO fully implement this. right now it is just a simulation for
   * performance numbers 
   */

  /* read a PTE, and store it to simulate obtaining the load_cr3 mapping */
  page_entry_t * page_entry = get_pgeVaddr (pg);
  page_entry_store(page_entry, *page_entry);
  sva_mm_flush_tlb (pg);

  /* simulate the branch */
  goto DOCHECK;
  
  //==-- Code residing on another set of pages --==//
  //
  /*
   * Check that the new page table is an L5 page table page.
   */
DOCHECK: 
  if ((mmuIsInitialized) && (getPageDescPtr(pg)->type != PG_L5)) {
    panic ("SVA: Loading non-L5 page into CR3: %lx %x\n", pg, getPageDescPtr (pg)->type);
  }

  _load_cr3(pg);

  /* Simulate return instruction */
  goto fini;

fini: 
  //==-- Simulate removing the mapping and then flush the TLB --==//
  page_entry = get_pgeVaddr (pg);
  page_entry_store(page_entry, *page_entry);
  sva_mm_flush_tlb (pg);

  
  MMULock_Release();
  return;
}

/*
 * Function: sva_load_cr0
 *
 * Description:
 *  SVA Intrinsic to load a value in cr0. We need to make sure write protection
 *  is enabled. 
 */
SECURE_WRAPPER(void,
sva_load_cr0, unsigned long val) {
    // No need to obtain MMU Lock
    val |= CR0_WP;
    _load_cr0(val);
    NK_ASSERT_PERF ((val & CR0_WP), "SVA: attempt to clear the CR0.WP bit: %x.",
        val);
}

/*
 * Function: sva_write_cr4
 *
 * Description:
 *  SVA Intrinsic to load a value in cr4. We need to make sure that the SMEP and PKS
 *  bits are enabled. 
 */
void sva_write_cr4(unsigned long val) {
  MMULock_Acquire();
  if(mmuIsInitialized) {
    val |= (1 << 24);
    printk("[mmu_init = 1] sva_write_cr4 = %lx\n", val);
  } else {
    printk("[mmu_init = 0] sva_write_cr4 = %lx\n", val);
  }
  _load_cr4(val);
  MMULock_Release();
}

/*
 * Function: sva_load_msr
 *
 * Description:
 *  SVA Intrinsic to load a value in an MSR. If the MSR is EFER, we need to
 *  make sure that the NXE bit is enabled. 
 */
void
sva_load_msr(u_int msr, uint64_t val) {
    if(msr == MSR_REG_EFER) {
        val |= EFER_NXE;
    }
    _wrmsr(msr, val);
    if ((msr == MSR_REG_EFER) && !(val & EFER_NXE)) {
      // panic("SVA: attempt to clear the EFER.NXE bit: %x.", val);
    }
}

/*
 * Function: sva_wrmsr
 *
 * Description:
 *  SVA Intrinsic to load a value in an MSR. The given value should be
 *  given in edx:eax and the MSR should be given in ecx. If the MSR is
 *  EFER, we need to make sure that the NXE bit is enabled. 
 */
// void
// sva_wrmsr() {
//     uint64_t val;
//     unsigned int msr;
//     __asm__ __volatile__ (
//         "wrmsr\n"
//         : "=c" (msr), "=a" (val)
//         :
//         : "rax", "rcx", "rdx"
//     );
//     if ((msr == MSR_REG_EFER) && !(val & EFER_NXE)) {
//       // panic("SVA: attempt to clear the EFER.NXE bit: %x.", val);
//     }
// }

/*
 * Function: declare_ptp_and_walk_pt_entries
 *
 * Descriptions:
 *  This function recursively walks a page table and it's entries to initalize
 *  the SVA data structures for the given page. This function is meant to
 *  initialize SVA data structures so they mirror the static page table setup
 *  by a kernel. However, it uses the paging structure itself to walk the
 *  pages, which means it should be agnostic to the operating system being
 *  employed upon. The function only walks into page table pages that are valid
 *  or enabled. It also makes sure that if a given page table is already active
 *  in SVA then it skips over initializing its entries as that could cause an
 *  infinite loop of recursion. This is an issue in FreeBSD as they have a
 *  recursive mapping in the pml4 top level page table page.
 *  
 *  If a given page entry is marked as having a larger page size, such as may
 *  be the case with a 2MB page size for PD entries, then it doesn't traverse
 *  the page. Therefore, if the kernel page tables are configured correctly
 *  this won't initialize any SVA page descriptors that aren't in use.
 *
 *  The primary objective of this code is to for each valid page table page:
 *      [1] Initialize the page_desc for the given page
 *      [2] Set the page permissions as read only
 *
 * Assumptions:
 *  - The number of entries per page assumes a amd64 paging hardware mechanism.
 *    As such the number of entires per a 4KB page table page is 2^9 or 512
 *    entries. 
 *  - This page referenced in pageMapping has already been determined to be
 *    valid and requires SVA metadata to be created.
 *
 * Inputs:
 *   pageMapping: Page mapping associated with the given page being traversed.
 *                This mapping identifies the physical address/frame of the
 *                page table page so that SVA can initialize it's data
 *                structures then recurse on each entry in the page table page. 
 *  numPgEntries: The number of entries for a given level page table. 
 *     pageLevel: The page level of the given mapping {1,2,3,4}.
 *
 *
 * TODO: 
 *  - Modify the page entry number to be dynamic in some way to accomodate
 *    differing numbers of entries. This only impacts how we traverse the
 *    address structures. The key issue is that we don't want to traverse an
 *    entry that randomly has the valid bit set, but not have it point to a
 *    real page. For example, if the kernel did not zero out the entire page
 *    table page and only inserted a subset of entries in the page table, the
 *    non set entries could be identified as holding valid mappings, which
 *    would then cause this function to traverse down truly invalid page table
 *    pages. In FreeBSD this isn't an issue given the way they initialize the
 *    static mapping, but could be a problem given differnet intialization
 *    methods.
 *
 *  - Add code to mark direct map page table pages to prevent the OS from
 *    modifying them.
 *
 */
#define DEBUG_INIT 0
void 
declare_ptp_and_walk_pt_entries(uintptr_t pageEntryPA, unsigned long
        numPgEntries, enum page_type_t pageLevel ) 
{ 
  int i;
  int traversedPTEAlready;
  enum page_type_t subLevelPgType;
  unsigned long numSubLevelPgEntries;
  page_desc_t *thisPg;
  page_entry_t pageMapping; 
  page_entry_t *pagePtr;

  /* Store the pte value for the page being traversed */
  pageMapping = pageEntryPA & PG_FRAME;

  /* Set the page pointer for the given page */
// #if USE_VIRT
  // uintptr_t pagePhysAddr = pageMapping & PG_FRAME;
  // pagePtr = (page_entry_t *) getVirtual(pagePhysAddr);
// #else
  pagePtr = (page_entry_t*) getVirtual((uintptr_t)(pageMapping & PG_FRAME));
// #endif

  /* Get the page_desc for this page */
  thisPg = getPageDescPtr(pageMapping);

  /* Mark if we have seen this traversal already */
  traversedPTEAlready = (thisPg->type != PG_UNUSED);

#if DEBUG_INIT >= 1
  /* Character inputs to make the printing pretty for debugging */
  char * indent = "";
  char * l5s = "L5:";
  char * l4s = "\tL4:";
  char * l3s = "\t\tL3:";
  char * l2s = "\t\t\tL2:";
  char * l1s = "\t\t\t\tL1:";

  switch (pageLevel){
    case PG_L5:
        indent = l5s;
        printk("%sSetting L5 Page: mapping:0x%lx\n", indent, pageMapping);
        break;
    case PG_L4:
        indent = l4s;
        printk("%sSetting L4 Page: mapping:0x%lx\n", indent, pageMapping);
        break;
    case PG_L3:
        indent = l3s;
        printk("%sSetting L3 Page: mapping:0x%lx\n", indent, pageMapping);
        break;
    case PG_L2:
        indent = l2s;
        printk("%sSetting L2 Page: mapping:0x%lx\n", indent, pageMapping);
        break;
    case PG_L1:
        indent = l1s;
        printk("%sSetting L1 Page: mapping:0x%lx\n", indent, pageMapping);
        break;
    default:
        break;
  }
#endif

  /*
   * For each level of page we do the following:
   *  - Set the page descriptor type for this page table page
   *  - Set the sub level page type and the number of entries for the
   *    recursive call to the function.
   */
  switch(pageLevel){

    case PG_L5:

      thisPg->type = PG_L5;       /* Set the page type to L4 */
      thisPg->user = 0;           /* Set the priv flag to kernel */
      ++(thisPg->count);
      subLevelPgType = PG_L4;
      numSubLevelPgEntries = NPGDEPG;//    numPgEntries;
      break;

    case PG_L4:

      thisPg->type = PG_L4;       /* Set the page type to L4 */
      thisPg->user = 0;           /* Set the priv flag to kernel */
      ++(thisPg->count);
      subLevelPgType = PG_L3;
      numSubLevelPgEntries = NP4DEPG;//    numPgEntries;
      break;

    case PG_L3:
      
      /* TODO: Determine why we want to reassign an L4 to an L3 */
      if (thisPg->type != PG_L4)
        thisPg->type = PG_L3;       /* Set the page type to L3 */
      thisPg->user = 0;           /* Set the priv flag to kernel */
      ++(thisPg->count);
      subLevelPgType = PG_L2;
      numSubLevelPgEntries = NPUDEPG; //numPgEntries;
      break;

    case PG_L2:
      
      /* 
       * If my L2 page mapping signifies that this mapping references a 1GB
       * page frame, then get the frame address using the correct page mask
       * for a L3 page entry and initialize the page_desc for this entry.
       * Then return as we don't need to traverse frame pages.
       */
      if ((pageMapping & PG_PS) != 0) {
#if DEBUG_INIT >= 1
        printk("\tIdentified 1GB page...\n");
#endif
        unsigned long index = (pageMapping & ~PUDMASK) / pageSize;
        page_desc[index].type = PG_TKDATA;
        page_desc[index].user = 0;           /* Set the priv flag to kernel */
        ++(page_desc[index].count);
        return;
      } else {
        thisPg->type = PG_L2;       /* Set the page type to L2 */
        thisPg->user = 0;           /* Set the priv flag to kernel */
        ++(thisPg->count);
        subLevelPgType = PG_L1;
        numSubLevelPgEntries = NPMDPG; // numPgEntries;
      }
      break;

    case PG_L1:
      /* 
       * If my L1 page mapping signifies that this mapping references a 2MB
       * page frame, then get the frame address using the correct page mask
       * for a L2 page entry and initialize the page_desc for this entry. 
       * Then return as we don't need to traverse frame pages.
       */
      if ((pageMapping & PG_PS) != 0){
#if DEBUG_INIT >= 1
        printk("\tIdentified 2MB page...\n");
#endif
        /* The frame address referencing the page obtained */
        unsigned long index = (pageMapping & ~PMDMASK) / pageSize;
        page_desc[index].type = PG_TKDATA;
        page_desc[index].user = 0;           /* Set the priv flag to kernel */
        ++(page_desc[index].count);
        return;
      } else {
        thisPg->type = PG_L1;       /* Set the page type to L1 */
        thisPg->user = 0;           /* Set the priv flag to kernel */
        ++(thisPg->count);
        subLevelPgType = PG_TKDATA;
        numSubLevelPgEntries = NPTEPG;//      numPgEntries;
      }
      break;

    default:
      panic("SVA: walked an entry with invalid page type.");
  }
  
  /* 
   * There is one recursive mapping, which is the last entry in the PML4 page
   * table page. Thus we return before traversing the descriptor again.
   * Notice though that we keep the last assignment to the page as the page
   * type information. 
   */
   // Rahul: Check how this translates to Linux
  if(traversedPTEAlready) {
#if DEBUG_INIT >= 1
    printk("%sRecursed on already initialized page_desc\n", indent);
#endif
    return;
  }

#if DEBUG_INIT >= 1
  u_long nNonValPgs=0;
  u_long nValPgs=0;
#endif
  /* 
   * Iterate through all the entries of this page, recursively calling the
   * walk on all sub entries.
   */
  for (i = 0; i < numSubLevelPgEntries; i++){
#if 0
    /*
     * Do not process any entries that implement the direct map.  This prevents
     * us from marking physical pages in the direct map as kernel data pages.
     */
    if ((pageLevel == PG_L4) && (i == (0xfffffe0000000000 / 0x1000))) {
      continue;
    }
#endif
#if OBSOLETE
    //pagePtr += (sizeof(page_entry_t) * i);
    //page_entry_t *nextEntry = pagePtr;
#endif
    page_entry_t * nextEntry = & pagePtr[i];

#if DEBUG_INIT >= 5
    printk("%sPagePtr in loop: %p, val: 0x%lx\n", indent, nextEntry, *nextEntry);
#endif

    /* 
     * If this entry is valid then recurse the page pointed to by this page
     * table entry.
     */
    if (*nextEntry & PG_V) {
#if DEBUG_INIT >= 1
      nValPgs++;
#endif 

      /* 
       * If we hit the level 1 pages we have hit our boundary condition for
       * the recursive page table traversals. Now we just mark the leaf page
       * descriptors.
       */
      if (pageLevel == PG_L1){
#if DEBUG_INIT >= 2
          printk("%sInitializing leaf entry: pteaddr: %p, mapping: 0x%lx\n",
                  indent, nextEntry, *nextEntry);
#endif
      } else {
#if DEBUG_INIT >= 2
      printk("%sProcessing:pte addr: %p, newPgAddr: %p, mapping: 0x%lx\n",
              indent, nextEntry, (*nextEntry & PG_FRAME), *nextEntry ); 
#endif
          // printk("[Next - %d]: %lx %lx", i, nextEntry, *nextEntry);
          declare_ptp_and_walk_pt_entries((uintptr_t)*nextEntry,
                  numSubLevelPgEntries, subLevelPgType); 
      }
    } 
#if DEBUG_INIT >= 1
    else {
      nNonValPgs++;
    }
#endif
  }

#if DEBUG_INIT >= 1
  SVA_ASSERT((nNonValPgs + nValPgs) == 512, "Wrong number of entries traversed");

  printk("%sThe number of || non valid pages: %lu || valid pages: %lu\n",
          indent, nNonValPgs, nValPgs);
#endif

}

/*
 * Function: init_protected_pages()
 *
 * Description:
 *  Set the PKS key for the virtual address range, and set the page type 
 *  for the physical page descriptor.
 *
 * Inputs: 
 *  startVA    - The first virtual address of the memory region.
 *  endVA      - The last virtual address of the memory region.
 *  pgType     - The nested kernel page type 
 */
static void
init_protected_pages (uintptr_t startVA, uintptr_t endVA) {
  uintptr_t page;
  for (page = startVA; page < endVA; page += pageSize) {
      // Set the physical page descriptor with the pgType

      // Set the PKS key for the virtual address
      pks_update_mapping(page, 1);

      // Flush the TLB for this virtual address
      sva_mm_flush_tlb(page);
  }
}

/*
 * Function: sva_mmu_init
 *
 * Description:
 *  This function initializes the sva mmu unit by zeroing out the page
 *  descriptors, capturing the statically allocated initial kernel mmu state,
 *  and identifying all kernel code pages, and setting them in the page
 *  descriptor array.
 *
 *  To initialize the sva page descriptors, this function takes the pml4 base
 *  mapping and walks down each level of the page table tree. 
 *
 *  NOTE: In this function we assume that he page mapping for the kpml4 has
 *  physical addresses in it. We then dereference by obtaining the virtual
 *  address mapping of this page. This works whether or not the processor is in
 *  a virtually addressed or physically addressed mode. 
 *
 * Inputs:
 *  - kpml4Mapping  : Mapping referencing the base kernel pml4 page table page
 *  - nkpml4e       : The number of entries in the pml4
 *  - firstpaddr    : A pointer to the physical address of the first free frame.
 *  - btext         : The first virtual address of the text segment.
 *  - etext         : The last virtual address of the text segment.
 */
SECURE_WRAPPER(void,
sva_mmu_init, void) {
  
  init_MMULock();

  MMULock_Acquire();

  /* Zero out the page descriptor array */
  // memset (page_desc, 0, numPageDescEntries * sizeof(page_desc_t));

  /* Walk the kernel page tables and initialize the sva page_desc */
  // declare_ptp_and_walk_pt_entries(__pa(kpgdVA), nkpgde, PG_L5);

  /* Protect the kernel text region */
  extern char _stext[];
  extern char _etext[];
  printk("_stext: %lx, _etext: %lx\n", _stext, _etext);
  init_protected_pages((uintptr_t) _stext, (uintptr_t)_etext);

  /* Make all SuperSpace pages read-only */
  extern char _svastart[];
  extern char _svaend[];
  printk("_svastart: %lx, _svaend: %lx\n", _svastart, _svaend);
  init_protected_pages((uintptr_t)_svastart, (uintptr_t)_svaend);
  
  /* Now load the initial value of the cr3 to complete kernel init */
  // _load_cr3(kpgdMapping->pgd & PG_FRAME);


  /* Make existing page table pages read-only */
  // makePTReadOnly();

  
  /* Make existing page table pages read-only */
  // makePTReadOnly();

  /*
   * Note that the MMU is now initialized.
   */
  mmuIsInitialized = 1;

  MMULock_Release();
}

void declare_internal(uintptr_t frameAddr, int level) {
  /* Get the page_desc for the page frame */
  page_desc_t *pgDesc = getPageDescPtr(frameAddr);

  /* Setup metadata tracking for this new page */
  pgDesc->type = level;

  /*
    * Reset the virtual address which can point to this page table page.
    */
  pgDesc->pgVaddr = 0;

  return;
}

/*
 * Intrinsic: sva_declare_l1_page()
 *
 * Description:
 *  This intrinsic marks the specified physical frame as a Level 1 page table
 *  frame.  It will zero out the contents of the page frame so that stale
 *  mappings within the frame are not used by the MMU.
 *
 * Inputs:
 *  frameAddr - The address of the physical page frame that will be used as a
 *              Level 1 page frame.
 */
SECURE_WRAPPER(void,
sva_declare_l1_page, uintptr_t frameAddr) {
  MMULock_Acquire();

  /* Get the page_desc for the newly declared l4 page frame */
  page_desc_t *pgDesc = getPageDescPtr(frameAddr);

  /*
   * Make sure that this is already an L1 page, an unused page, or a kernel
   * data page.
   */
  switch (pgDesc->type) {
    case PG_UNUSED:
    case PG_L1:
    case PG_TKDATA:
      break;

    default:
      panic ("SVA: Declaring L1 for wrong page: frameAddr = %lx, pgDesc=%lx, type=%x\n", frameAddr, pgDesc, pgDesc->type);
      break;
  }

  /* 
   * Declare the page as an L1 page (unless it is already an L1 page).
   */
  if (pgDesc->type != PG_L1) {
    /*
     * Mark this page frame as an L1 page frame.
     */
    pgDesc->type = PG_L1;

    /*
     * Reset the virtual address which can point to this page table page.
     */
    pgDesc->pgVaddr = 0;

    /* 
     * Initialize the page data and page entry. Note that we pass a general
     * page_entry_t to the function as it enables reuse of code for each of the
     * entry declaration functions. 
     */
    initDeclaredPage(frameAddr);
  } else {
    panic ("SVA: declare L1: type = %x\n", pgDesc->type);
  }

  MMULock_Release();
  return;
}

/*
 * Intrinsic: sva_declare_l2_page()
 *
 * Description:
 *  This intrinsic marks the specified physical frame as a Level 2 page table
 *  frame.  It will zero out the contents of the page frame so that stale
 *  mappings within the frame are not used by the MMU.
 *
 * Inputs:
 *  frameAddr - The address of the physical page frame that will be used as a
 *              Level 2 page frame.
 */
SECURE_WRAPPER(void, 
sva_declare_l2_page, uintptr_t frameAddr) {
  MMULock_Acquire();

  /* Get the page_desc for the newly declared l2 page frame */
  page_desc_t *pgDesc = getPageDescPtr(frameAddr);

  /*
   * Make sure that this is already an L2 page, an unused page, or a kernel
   * data page.
   */
  switch (pgDesc->type) {
    case PG_UNUSED:
    case PG_L2:
    case PG_TKDATA:
      break;

    default:
      panic ("SVA: Declaring L2 for wrong page: frameAddr = %lx, pgDesc=%lx, type=%x count=%x\n", frameAddr, pgDesc, pgDesc->type, pgDesc->count);
      break;
  }


  /* 
   * Declare the page as an L2 page (unless it is already an L2 page).
   */
  if (pgDesc->type != PG_L2) {
    /* Setup metadata tracking for this new page */
    pgDesc->type = PG_L2;

    /*
     * Reset the virtual address which can point to this page table page.
     */
    pgDesc->pgVaddr = 0;

    /* 
     * Initialize the page data and page entry. Note that we pass a general
     * page_entry_t to the function as it enables reuse of code for each of the
     * entry declaration functions. 
     */
    initDeclaredPage(frameAddr);
  } else {
    panic ("SVA: declare L2: type = %x\n", pgDesc->type);
  }

  MMULock_Release();
  return;
}


/*
 * Intrinsic: sva_declare_l3_page()
 *
 * Description:
 *  This intrinsic marks the specified physical frame as a Level 3 page table
 *  frame.  It will zero out the contents of the page frame so that stale
 *  mappings within the frame are not used by the MMU.
 *
 * Inputs:
 *  frameAddr - The address of the physical page frame that will be used as a
 *              Level 3 page frame.
 */
SECURE_WRAPPER(void,
sva_declare_l3_page, uintptr_t frameAddr) {
  MMULock_Acquire();

  /* Get the page_desc for the newly declared l4 page frame */
  page_desc_t *pgDesc = getPageDescPtr(frameAddr);

  /*
   * Make sure that this is already an L3 page, an unused page, or a kernel
   * data page.
   */
  switch (pgDesc->type) {
    case PG_UNUSED:
    case PG_L3:
    case PG_TKDATA:
      break;

    default:
      panic ("SVA: Declaring L3 for wrong page: frameAddr = %lx, pgDesc=%lx, type=%x count=%x\n", frameAddr, pgDesc, pgDesc->type, pgDesc->count);
      break;
  }

  /* 
   * Declare the page as an L3 page (unless it is already an L3 page).
   */
  if (pgDesc->type != PG_L3) {
    /* Mark this page frame as an L3 page frame */
    pgDesc->type = PG_L3;

    /*
     * Reset the virtual address which can point to this page table page.
     */
    pgDesc->pgVaddr = 0;

    /* 
     * Initialize the page data and page entry. Note that we pass a general
     * page_entry_t to the function as it enables reuse of code for each of the
     * entry declaration functions. 
     */
    initDeclaredPage(frameAddr);
  } else {
    panic ("SVA: declare L3: type = %x\n", pgDesc->type);
  }

  MMULock_Release();
  return;
}

/*
 * Intrinsic: sva_declare_l4_page()
 *
 * Description:
 *  This intrinsic marks the specified physical frame as a Level 4 page table
 *  frame.  It will zero out the contents of the page frame so that stale
 *  mappings within the frame are not used by the MMU.
 *
 * Inputs:
 *  frameAddr - The address of the physical page frame that will be used as a
 *              Level 4 page frame.
 */
SECURE_WRAPPER(void,
sva_declare_l4_page, uintptr_t frameAddr) {
  MMULock_Acquire();
  /* Get the page_desc for the newly declared l4 page frame */
  page_desc_t *pgDesc = getPageDescPtr(frameAddr);

  /*
   * Make sure that this is already an L4 page, an unused page, or a kernel
   * data page.
   */
  switch (pgDesc->type) {
    case PG_UNUSED:
    case PG_L4:
    case PG_TKDATA:
      break;

    default:
      panic ("SVA: Declaring L4 for wrong page: frameAddr = %lx, pgDesc=%lx, type=%x\n", frameAddr, pgDesc, pgDesc->type);
      break;
  }

  /* 
   * Declare the page as an L4 page (unless it is already an L4 page).
   */
  if (pgDesc->type != PG_L4) {
    /* Mark this page frame as an L4 page frame */
    pgDesc->type = PG_L4;

    // unsigned long vaddr = __va(frameAddr);
    // vaddr = (vaddr >> 12) << 12;
    // printk("SVA_declare_L4 %lx %lx\n", frameAddr, vaddr);
    // pks_update_mapping(vaddr, 1);
    // sva_mm_flush_tlb(vaddr);

    /*
     * Reset the virtual address which can point to this page table page.
     */
    pgDesc->pgVaddr = 0;

    /* 
     * Initialize the page data and page entry. Note that we pass a general
     * page_entry_t to the function as it enables reuse of code for each of the
     * entry declaration functions. 
     */
    initDeclaredPage(frameAddr);
  } else {
    panic ("SVA: declare L4: type = %x\n", pgDesc->type);
  }
  MMULock_Release();
}

/*
 * Intrinsic: sva_declare_l5_page()
 *
 * Description:
 *  This intrinsic marks the specified physical frame as a Level 5 page table
 *  frame.  It will zero out the contents of the page frame so that stale
 *  mappings within the frame are not used by the MMU.
 *
 * Inputs:
 *  frameAddr - The address of the physical page frame that will be used as a
 *              Level 5 page frame.
 */
SECURE_WRAPPER(void,
sva_declare_l5_page, uintptr_t frameAddr) {
  MMULock_Acquire();
  /* Get the page_desc for the newly declared l4 page frame */
  page_desc_t *pgDesc = getPageDescPtr(frameAddr);

  /*
   * Make sure that this is already an L4 page, an unused page, or a kernel
   * data page.
   */
  switch (pgDesc->type) {
    case PG_UNUSED:
    case PG_L5:
    case PG_TKDATA:
      break;

    default:
      panic ("SVA: Declaring L5 for wrong page: frameAddr = %lx, pgDesc=%lx, type=%x\n", frameAddr, pgDesc, pgDesc->type);
      break;
  }

  /* 
   * Declare the page as an L4 page (unless it is already an L4 page).
   */
  if (pgDesc->type != PG_L5) {
    /* Mark this page frame as an L4 page frame */
    pgDesc->type = PG_L5;

    /*
     * Reset the virtual address which can point to this page table page.
     */
    pgDesc->pgVaddr = 0;

    /* 
     * Initialize the page data and page entry. Note that we pass a general
     * page_entry_t to the function as it enables reuse of code for each of the
     * entry declaration functions. 
     */
    initDeclaredPage(frameAddr);
  } else {
    panic ("SVA: declare L5: type = %x\n", pgDesc->type);
  }
  MMULock_Release();
}


/*
 * Function: sva_remove_page()
 *
 * Description:
 *  This function informs the SVA VM that the system software no longer wants
 *  to use the specified page as a page table page.
 *
 * Inputs:
 *  paddr - The physical address of the page table page.
 */
SECURE_WRAPPER(void,
sva_remove_page, uintptr_t paddr) {
  MMULock_Acquire();

  /* Get the entry controlling the permissions for this pte PTP */
  page_entry_t *pte = get_pgeVaddr(getVirtual (paddr));

  /* Get the page_desc for the page frame */
  page_desc_t *pgDesc = getPageDescPtr(paddr);

  /*
   * Make sure that this is a page table page.  We don't want the system
   * software to trick us.
   */
  switch (pgDesc->type)  {
    case PG_L1:
    case PG_L2:
    case PG_L3:
    case PG_L4:
    case PG_L5:
      break;

    default:
      /* Restore interrupts */
      // Rahul: 
      // The Linux kernel does not seem to use a specific PTP free fucntion to free a page tbale page
      // Hence we call an sva_remove page on all memory frees. For now, just ignoring this panic.
      // TODO: Check if this leads to a significant performance overhead.
      // panic ("SVA: undeclare bad page type: %lx %lx\n", paddr, pgDesc->type);
      MMULock_Release();
      return;
      break;
  }

  /*
   * Check that there are no references to this page (i.e., there is no page
   * table entry that refers to this physical page frame).  If there is a
   * mapping, then someone is still using it as a page table page.  In that
   * case, ignore the request.
   *
   * Note that we check for a reference count of 1 because the page is always
   * mapped into the direct map.
   */
  if ((pgDesc->count == 1) || (pgDesc->count == 0)) {
    /*
     * Mark the page frame as an unused page.
     */
    pgDesc->type = PG_UNUSED;

    // unsigned long vaddr = __va(paddr);
    // vaddr = (vaddr >> 12) << 12;
    // printk("SVA_declare_L4 %lx %lx\n", frameAddr, vaddr);
    // pks_update_mapping(vaddr, 0);

    /*
     * Make the page writeable again.  Be sure to flush the TLBs to make the
     * change take effect right away.
     */
    // page_entry_store ((page_entry_t *) pte, setMappingReadWrite (*pte));
    // sva_mm_flush_tlb (getVirtual (paddr));
  } else {
    panic ("SVA: remove_page: type=%x count %x\n", pgDesc->type, pgDesc->count);
  }
 
  MMULock_Release();
  return;
}

/* 
 * Function: sva_remove_mapping()
 *
 * Description:
 *  This function updates the entry to the page table page and is agnostic to
 *  the level of page table. The particular needs for each page table level are
 *  handled in the __update_mapping function. The primary function here is to
 *  set the mapping to zero, if the page was a PTP then zero it's data and set
 *  it to writeable.
 *
 * Inputs:
 *  pteptr - The location within the page table page for which the translation
 *           should be removed.
 */
SECURE_WRAPPER(void,
sva_remove_mapping, page_entry_t *pteptr) {
  MMULock_Acquire();

  /* Get the page_desc for the newly declared l4 page frame */
  // page_desc_t *pgDesc = getPageDescPtr(*pteptr); // Rahul: What's happening here ?

  /* Update the page table mapping to zero */
  __update_mapping (pteptr, ZERO_MAPPING);

  MMULock_Release();
}

#define DECLARE_STRATEGY 2

/* 
 * Function: sva_update_l1_mapping()
 *
 * Description:
 *  This function updates a Level-1 Mapping.  In other words, it adds a
 *  a direct translation from a virtual page to a physical page.
 *
 *  This function makes different checks to ensure the mapping
 *  does not bypass the type safety proved by the compiler.
 *
 * Inputs:
 *  pteptr - The location within the L1 page in which the new translation
 *           should be place.
 *  val    - The new translation to insert into the page table.
 */
SECURE_WRAPPER(void,
sva_update_l1_mapping, pte_t *pte, page_entry_t val) {
  MMULock_Acquire();
  /*
   * Ensure that the PTE pointer points to an L1 page table.  If it does not,
   * then report an error.
   */
  void *p = pte;
  page_desc_t * ptDesc = getPageDescPtr (__pa(pte));

  #if DECLARE_STRATEGY == 1
  #endif

  #if DECLARE_STRATEGY == 2
    if(ptDesc->type != PG_L1 && ptDesc->type == PG_UNUSED) {
      declare_internal(__pa(pte), 1);
    }
  #endif

  if (ptDesc->type != PG_L1) {
    panic ("SVA: MMU: update_l1 not an L1: %lx %lx: %lx\n", &pte->pte, val, ptDesc->type);
  }

  /*
   * Update the page table with the new mapping.
   */
  if(getPageDescPtr((val & PTE_PFN_MASK) != -1))
    __update_mapping(&pte->pte, val);

  MMULock_Release();
  return;
}

/*
 * Updates a level2 mapping (a mapping to a l1 page).
 *
 * This function checks that the pages involved in the mapping
 * are correct, ie pmdptr is a level2, and val corresponds to
 * a level1.  
 */
SECURE_WRAPPER(void,
sva_update_l2_mapping, pmd_t *pmd, page_entry_t val) {
  MMULock_Acquire();
  /*
   * Ensure that the PTE pointer points to an L2 page table.  If it does not,
   * then report an error.
   */

  page_desc_t * ptDesc = getPageDescPtr (__pa(pmd));

  #if DECLARE_STRATEGY == 1
    uintptr_t nextPagePaddr = (val >> 12) << 12;
    page_desc_t * nextPagePtDesc = getPageDescPtr (nextPagePaddr);
    if(nextPagePtDesc->type != PG_L1 && nextPagePtDesc->type == PG_UNUSED) {
      declare_internal(nextPagePaddr, 1);
    }
  #endif

  #if DECLARE_STRATEGY == 2
    if(ptDesc->type != PG_L2 && ptDesc->type == PG_UNUSED) {
      declare_internal(__pa(pmd), 2);
    }
  #endif

  if (ptDesc->type != PG_L2) {
    panic ("SVA: MMU: update_l2 not an L2: %lx %lx: type=%lx count=%lx\n", &pmd->pmd, val, ptDesc->type, ptDesc->count);
  }

  /*
   * Update the page mapping.
   */
  __update_mapping(&pmd->pmd, val);

  MMULock_Release();
  return;
}

/*
 * Updates a level3 mapping 
 */
SECURE_WRAPPER(void, sva_update_l3_mapping, pud_t * pud, page_entry_t val) {
  MMULock_Acquire();
  /*
   * Ensure that the PTE pointer points to an L3 page table.  If it does not,
   * then report an error.
   */
  page_desc_t * ptDesc = getPageDescPtr (__pa(&pud->pud));

  #if DECLARE_STRATEGY == 1
    uintptr_t nextPagePaddr = (val >> 12) << 12;
    page_desc_t * nextPagePtDesc = getPageDescPtr (nextPagePaddr);
    if(nextPagePtDesc->type != PG_L2 && nextPagePtDesc->type == PG_UNUSED) {
      declare_internal(nextPagePaddr, 2);
    }
  #endif

  #if DECLARE_STRATEGY == 2
    if(ptDesc->type != PG_L3 && ptDesc->type == PG_UNUSED) {
      declare_internal(__pa(pud), 3);
    }
  #endif

  if (ptDesc->type != PG_L3) {
    panic ("SVA: MMU: update_l3 not an L3: %lx %lx: %lx\n", &pud->pud, val, ptDesc->type);
  }

  __update_mapping(&pud->pud, val);

  MMULock_Release();
  return;
}

/*
 * Updates a level4 mapping 
 */
SECURE_WRAPPER( void, sva_update_l4_mapping ,p4d_t * p4d, page_entry_t val) {
  MMULock_Acquire();
  /*
   * Ensure that the PTE pointer points to an L4 page table.  If it does not,
   * then report an error.
   */
  page_desc_t * ptDesc = getPageDescPtr (__pa(&p4d->p4d));

  #if DECLARE_STRATEGY == 1
    uintptr_t nextPagePaddr = (val >> 12) << 12;
    page_desc_t * nextPagePtDesc = getPageDescPtr (nextPagePaddr);
    if(nextPagePtDesc->type != PG_L3 && nextPagePtDesc->type == PG_UNUSED) {
      declare_internal(nextPagePaddr, 3);
    }
  #endif

  #if DECLARE_STRATEGY == 2
    if(ptDesc->type != PG_L4 && ptDesc->type == PG_UNUSED) {
      // unsigned long vaddr = &p4d->p4d;
      // vaddr = (vaddr >> 12) << 12;
      // pks_update_mapping(vaddr, 1);
      declare_internal(__pa(p4d), 4);
    }
  #endif
  
  if (ptDesc->type != PG_L4) {
    panic ("SVA: MMU: update_l4 not an L4: %lx %lx: %lx\n", &p4d->p4d, val, ptDesc->type);
  }

  __update_mapping(&p4d->p4d, val);

  MMULock_Release();
  return;
}

/*
 * Updates a level5 mapping 
 */
SECURE_WRAPPER( void, sva_update_l5_mapping, pgd_t * pgd, page_entry_t val) {
  MMULock_Acquire();
  /*
   * Ensure that the PTE pointer points to an L5 page table.  If it does not,
   * then report an error.
   */  
  page_desc_t * ptDesc = getPageDescPtr (__pa(&pgd->pgd));

  #if DECLARE_STRATEGY == 1
    uintptr_t nextPagePaddr = (val >> 12) << 12;
    page_desc_t * nextPagePtDesc = getPageDescPtr (nextPagePaddr);  
    if(nextPagePtDesc->type != PG_L4 && nextPagePtDesc->type == PG_UNUSED) {
      declare_internal(nextPagePaddr, 4);
    }
  #endif

  #if DECLARE_STRATEGY == 2
    if(ptDesc->type != PG_L5 && ptDesc->type == PG_UNUSED) {
      // unsigned long vaddr = &pgd->pgd;
      // vaddr = (vaddr >> 12) << 12;
      // pks_update_mapping(vaddr, 1);
      declare_internal(__pa(pgd), 5);
    }
  #endif

  if (ptDesc->type != PG_L5) {
    panic ("SVA: MMU: update_l5 not an L5: %lx %lx: %lx\n", &pgd->pgd, val, ptDesc->type);
  }

  __update_mapping(&pgd->pgd, val);

  MMULock_Release();
  return;
}

int getPageType(uintptr_t pfn) {
  return getPageDescPtr(pfn)->type;
}