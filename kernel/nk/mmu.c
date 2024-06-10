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
#include <asm/tlbflush.h>
#include <asm-generic/rwonce.h>

#include "sva/config.h"
#include "sva/mmu.h"
#include "sva/mmu_intrinsics.h"
#include "sva/stack.h"
#include "sva/svamem.h"
#include "sva/x86.h"
#include "sva/pks.h"
#include "sva/idt.h"
#include "sva/enc.h"

#include <asm/smap.h>


/* Special panic(..) that also prints the outer kernel's stack */
extern char _svastart[];
extern char _svaend[];

/* SM functions */
extern char __svamem_text_start[];
extern char __svamem_text_end[];

/* sensitive privile function section */
extern char __svamem_priv_text_start[];
extern char __svamem_priv_text_end[];
unsigned long *__svamem_priv_text_pte = NULL;

/* overall kernel functions */
extern char _stext[];
extern char _etext[];

static inline void print_insecure_stack(void) {
  show_stack(current, get_insecure_context_rsp(), KERN_DEFAULT);
}

#define PANIC(args ...) \
  printk("-----------------------------------------------"); \
  print_insecure_stack(); \
  printk("-----------------------------------------------"); \
  panic(args)

#define PANIC_WRONG_MAPPING(args ...) \
  printk("-----------------------------------------------"); \
  print_insecure_stack(); \
  printk("-----------------------------------------------"); \
  LOG_PRINTK("_stext: %lx, _etext: %lx\n", __pa(_stext), __pa(_etext)); \
  LOG_PRINTK("_svastart: %lx, _svaend: %lx\n", __pa(_svastart), __pa(_svaend)); \
  printk("-----------------------------------------------"); \
  panic(args)

/* 
 * Description: 
 *   This is a pointer to the secure monitor's runtime stack, which is used on
 *   calls to SM.
 */
char SecureStack[pageSize*NCPU] __attribute__((aligned(0x1000))) SVAMEM;

/* Chuqi: Important this value can't be changed from outside the nested kernel! 
 * Remember the stack grows down, so the base is the highest address.
 */
const unsigned long SecureStackBase = (unsigned long) SecureStack + pageSize;

char data_page[4096] __attribute__((aligned(0x1000))) SVAMEM;

/*
 * Chuqi: TODO: protect its memory
 */
// char SyscallSecureStack[4096*NCPU] __attribute__((aligned(0x1000)));
char SyscallSecureStack[pageSize*NCPU] __attribute__((aligned(0x1000))) SVAMEM;

const uintptr_t SyscallSecureStackBase = (uintptr_t) SyscallSecureStack + pageSize;


#undef NKDEBUGG
#define NKDEBUGG(fname, fmt, args...) /* nothing: it's a placeholder */


/*
 *****************************************************************************
 * MSR and control register (CR) operations.
 *****************************************************************************
 */
void SVATEXT_PRIV
_load_cr0(unsigned long val) {
	__asm __volatile("movq %0,%%cr0" : : "r" (val));
}

void SVATEXT_PRIV
_load_cr3(unsigned long data)
{ 
	__asm __volatile("movq %0,%%cr3" : : "r" (data) : "memory"); 
}

void SVATEXT_PRIV
_load_cr4(unsigned long val) {
	// __asm __volatile("movq %0,%%cr4" : : "r" (val) : "memory");
	asm volatile("mov %0,%%cr4": "+r" (val) : : "memory");
}

void
_wrmsr(unsigned long msr, uint64_t newval)
{
	uint32_t low, high;

	low = newval;
	high = newval >> 32;
	__asm __volatile("wrmsr" : : "a" (low), "d" (high), "c" (msr));
}

uint64_t
_rdmsr(unsigned long msr)
{
	uint32_t low, high;

	__asm __volatile("rdmsr" : "=a" (low), "=d" (high) : "c" (msr));
	return (low | ((uint64_t)high << 32));
}

unsigned long
_rcr0(void) {
	unsigned long  data;
	__asm __volatile("movq %%cr0,%0" : "=r" (data));
	return (data);
}

unsigned long
_rcr3(void) {
	unsigned long  data;
	__asm __volatile("movq %%cr3,%0" : "=r" (data));
	return (data);
}

unsigned long
_rcr4(void) {
	unsigned long  data;
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
static  void __update_mapping (unsigned long * pageEntryPtr, page_entry_t val);

/*
 *****************************************************************************
 * Define paging structures and related constants local to this source file
 *****************************************************************************
 */

/* Flags whether the MMU has been initialized */
unsigned char mmuIsInitialized = 0;


/* Array describing the physical pages */
/* The index is the physical page number */
static page_desc_t page_desc[numPageDescEntries] SVAMEM;



/*
 * Object: MMULock
 *
 * Description:
 *  This is the spinlock used for synchronizing access to the page tables.
 *  Chuqi: TODO: replace the normal OS's spinlock with our own spinlock.
 * 
 * Chuqi: check. Since all `spin_lock_init`, `spin_lock`, and `spin_unlock`
 * seem to be inline functions or macros, it might be safe to use them now.
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
 *  the frame being addressed in the mapping. For AMD SEV VMs, this also removes 
 *  the c-bit (51) from the physical address
 *
 * Inputs:
 *  mapping: the mapping with the physical address of the referenced frame
 *
 * Return:
 *  Pointer to the page_desc for this frame
 */
page_desc_t * getPageDescPtr(unsigned long mapping) {

  unsigned long frameIndex = pageEntryToPA(mapping, /*is_to_frame=*/1);
  /* Sanity check */
  if(frameIndex  >= numPageDescEntries) {
	/* Basically, this is to deal with the kernel's junk mappings */
	// LOG_PRINTK("[PANIC]: SVA: getPageDescPtr: %lx %lx %lx\n", mapping, frameIndex, numPageDescEntries);
	return NULL;
  }
  return page_desc + frameIndex;
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
 * - Chuqi: s/WP/PKS
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
  /* 
   * ENCOS: Using Linux primitive. No need to flush ( __flush_tlb_all();), since 
   * the kernel should automatically handle flushing after return.
   */
  WRITE_ONCE(*page_entry, newVal);
}


/*
 * Function: sm_entry_map_priv_page / sm_exit_unmap_priv_page
 *
 * Description:
 *  These functions are a part of the entry/exit gates for the SM. The reason
 *  is that both PKS/WP only protects memory R/W, but not X. Thus, a malicious
 *  OS can still chain a gadget and jumps to the SM function to perform a 
 *  privileged operation. Think about this:
 *  A malicious OS jumps to _load_cr3 and changes the CR3 mapping. After that,
 *  the exit gate is mapped away and the OS is no longer restricted by SM.
 *  
 *  Therefore, these functions are used to map/unmap the sensitive privilege 
 *  sections, which contain privileged operations (CR0-CR4 and MSR writing), 
 *  in the kernel. Only the entry gate can remap them back when legally entering
 *  the SM.
 */
/* map/unmap privilege sections */
void SVATEXT
sm_entry_map_priv_page(void) 
{
  int level;
  if (!mmuIsInitialized)
	return;
  MMULock_Acquire();
  if (!__svamem_priv_text_pte) {
	__svamem_priv_text_pte = get_pgeVaddr((unsigned long)__svamem_priv_text_start, &level);
	if (level != 1){ /* won't happen */
	  __svamem_priv_text_pte = NULL;
	  MMULock_Release();
	  return;
	}
  }
  /* map the privilege code page */
  page_entry_store(__svamem_priv_text_pte, *__svamem_priv_text_pte | PG_V);
  /* Chuqi: check, printk cannot work: 
   * causes double fault right after the boot message
   * "SLUB: HWalign=64, Order=0-3, MinObjects=0, CPUs=8, Nodes=1"
   */
  // printk("[map] priv_text start VA=0x%lx, PTEaddr=0x%lx, PTEval=0x%lx.\n",
  //       (unsigned long)__svamem_priv_text_start, __svamem_priv_text_pte,
  //       *__svamem_priv_text_pte);
  sva_mm_flush_tlb(__svamem_priv_text_start);
  MMULock_Release();
  return;
}

void SVATEXT
sm_exit_unmap_priv_page(void)
{
  int level;
  if (!mmuIsInitialized)
	return;
  MMULock_Acquire();
  if (!__svamem_priv_text_pte) {
	__svamem_priv_text_pte = get_pgeVaddr((unsigned long)__svamem_priv_text_start, &level);
	if (level != 1){ /* won't happen */
	  __svamem_priv_text_pte = NULL;
	  MMULock_Release();
	  return;
	}
  }
  /* unmap the privilege code page */
  page_entry_store(__svamem_priv_text_pte, *__svamem_priv_text_pte & ~PG_V);
  /* Chuqi: check, printk cannot work: 
   * causes double fault right after the boot message
   * "SLUB: HWalign=64, Order=0-3, MinObjects=0, CPUs=8, Nodes=1"
   */
  // printk("[unmap] priv_text start VA=0x%lx PTEaddr=0x%lx, PTEval=0x%lx.\n",
  //       (unsigned long)__svamem_priv_text_start, __svamem_priv_text_pte,
  //       *__svamem_priv_text_pte);
  sva_mm_flush_tlb(__svamem_priv_text_start);
  MMULock_Release();
  return;
}

/*
 *****************************************************************************
 * Page table page index and entry lookups 
 *****************************************************************************
 */

/*
 * Function: sm_validate_pt_update()
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
 * A success return means the security check is passed, and the mapping can be 
 * performed.
 *  0  - The update is not valid and should not be performed.
 *  1  - The update is valid but should disable write access.
 *  2  - The update is valid and can be performed.
 * 
 * Otherwise, this function directly PANICs the kernel.
 */
static unsigned char
sm_validate_pt_update (page_entry_t *page_entry, page_entry_t newVal) {
  /* Collect associated information for the existing mapping */
  unsigned long origPA = pageEntryToPA(*page_entry, /*is_to_frame=*/0);
  unsigned long origFrame = origPA >> PAGE_SHIFT;
  unsigned long origVA = (unsigned long) getVirtual(origPA);
  page_desc_t *origPG = getPageDescPtr(origPA);

  /* Get associated information for the new page being mapped */
  unsigned long newPA = pageEntryToPA(newVal, /*is_to_frame=*/0);
  unsigned long newFrame = newPA >> PAGE_SHIFT;
  unsigned long newVA = (unsigned long) getVirtual(newPA);
  page_desc_t *newPG = getPageDescPtr(newPA);

  /* Get the page table page descriptor. The page_entry is the viratu */
  unsigned long ptePAddr = __pa (page_entry);
  page_desc_t *ptePG = getPageDescPtr(ptePAddr);

  /* Return value */
  unsigned char retValue = 2;

  /* =======================================================================
   * Deal with SM code/data page remappings by last-level L1 PTEs
   * ======================================================================= */
  if (isSMPg(origPG)) {
	/* do not allow remap a sensitive page from the last level */
	if ((origPA != newPA) && ptePG->type == PG_L1) {
	  PANIC("Invalid updating [origPA=0x%lx (type=%d) -> newPA=0x%lx (type=%d)]. OriginPA is sensitive.\n", 
		  origPA, origPG->type, newPA, newPG->type);
	}
  }
  /* =======================================================================
   * Deal with SM code/data page new-mappings protection
   * ======================================================================= */
  if (isSMPg(newPG)) {
	/* if they are the same mapping, we preserve the protection */
	if (!__check_pte_protection(&newVal)) {
	  /* do_mmu_update will flush the TLB later on */
	  __set_pte_protection(&newVal, /*should_protect=*/1);
	}
  }

  /* 
   * We don't get the targetPA's page descriptor. In this case, the target 
   * mapping is a junk page ("invalid" physical page). Kernel wants to use 
   * this junk mapping, we just let it go.
   * 
   * Here is a clear example why kernel uses a junk mapping to an "weird PA":
   * When a user requests mprotect(PROT_NONE) (none-access to a mapping), the 
   * kernel will update VA -> a junk PA (like targetPA=0xffff_ffff_ffff_ffff 
   * which is out of the PA's range).
   */
  if(!newPG) { return 0; }

  /* 
   * If we aren't mapping a new page then we can skip several checks, and in
   * some cases we must, otherwise, the checks will fail. For example if this
   * is a mapping in a page table page then we allow a zero mapping. 
   */
  if (newVal & PG_V) {
	/* If the mapping is to an SVA page then fail */
	SVA_ASSERT (!(isSVAPg(newPG) && (origPA != newPA)), 
			  "Kernel attempted to remap an SVA page {target pgEntry_VA=0x%lx, origPTE=0x%lx->newPTE=0x%lx,\n}",
			  (unsigned long)page_entry, (unsigned long)*page_entry, newVal);

	/*
	 * New mappings to code pages are permitted as long as they are either
	 * for user-space pages or do not permit write access.
	 */
	if (isCodePg (newPG)) {
	  if ((newVal & (PG_RW | PG_U)) == (PG_RW)) {
		PANIC ("SVA: Making kernel code writeable: %lx %lx\n", newVA, newVal);
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
	  if (pgRefCount(newPG) > 1) {
		// if (newPG->pgVaddr != page_entry) {
		  // PANIC("Map PTP to 2nd VA (oldVA: %lx newVA:%lx, PA:%px, type=%x\n", 
			  // newPG->pgVaddr, page_entry, ptePAddr, newPG->type);
		// }
	  } else {
		newPG->pgVaddr = page_entry;
	  }
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
			/* 
			 * ENCOS: Another compromise made here. Basically, this is the case
			 * where the kernel will split hugepages for SVA regions (and others?).
			 * In such cases, we allow split mappings, but enable protection on 
			 * child entries.
			 */
			if (newPG->type >= PG_SVA) {
			  /* TODO: Let's add the read-only bit. */
			  retValue = 2;
			} else {
			  PANIC_WRONG_MAPPING ("SVA: Map bad page type into L1: (virt: %px, phys: %px, pte: %px, pgtype: %x)\n", 
				newVA, newPA, ptePAddr, newPG->type);
			}
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
			/* 
			 * ENCOS: Another compromise made here. Basically, this is the case
			 * where the kernel will split hugepages for SVA regions (and others?).
			 * In such cases, we allow split mappings, but enable protection on 
			 * child entries.
			 */
			  if (newPG->type >= PG_SVA) {
				/* TODO: Let's add the read-only bit. */
				retValue = 2;
			  } else {
				PANIC_WRONG_MAPPING ("SVA: Map bad page type into L2: (virt: %px, phys: %px, pte: %px, pgtype: %x)\n", 
				newVA, newPA, ptePAddr, newPG->type);
			  }
			}
		  }
		} else {
		  // ENCOS: TODO (check why this fails.)
		  // SVA_ASSERT (isL1Pg(newPG), "MMU: Mapping non-L1 page into L2.");
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
			  PANIC_WRONG_MAPPING ("SVA: Map bad page type into L3: (virt: %px, phys: %px, pte: %px, pgtype: %x)\n", 
				newVA, newPA, ptePAddr, newPG->type);
			}
		  }
		} else {
		  // SVA_ASSERT (isL2Pg(newPG), "SVA: Mapping non-L2 page into L3.");
		}
		break;

	  case PG_L4:
		SVA_ASSERT (isL3Pg(newPG), "SVA: Mapping non-L3 page into L4.");
		break;

#if defined (CONFIG_X86_5LEVEL)
	  case PG_L5:
		SVA_ASSERT (isL4Pg(newPG), "SVA: Mapping non-L4 page into L5.");
		break;
#endif

	  default:
		PANIC_WRONG_MAPPING("SVA: Incorrect mapping that does not belong to L1 - L4/5");
		break;
	}
  }

  /* =======================================================================
   * Deal with the enclave PTP mapping chain check.
   * ======================================================================= */
  if (ptePG->encID && !newPG->encID) {
	/*  
	  * for L1_PTE -> PA. In this case, we are handling an undeclared memory 
	  * in enclave, we revoke the write-permission.
	  */
	if (ptePG->type == PG_L1) { newVal &= ~PG_RW; }
	/*  
	  * for Lx_PTE -> L(x-1)_PTP. In this case, we are handling enclave PTP 
	  * chaining, we clear the target PTP memory first to avoid malicious mapping.
	  */
	else { memset((void *)newVA, 0, PAGE_SIZE); }  
	newPG->encID = ptePG->encID;
  }
  else if (!ptePG->encID && newPG->encID) {
	// memset((void *)((unsigned long)page_entry & addrmask), 0, PAGE_SIZE);
	ptePG->encID = newPG->encID;
  } 
  else if (!ptePG->encID && !newPG->encID) {
	/* 
	 * check undeclared PA and reverse-order chaining,
	 * we revoke the write-permission.
	 */
	if (ptePG->type == PG_L1 && current_encid()) {
	  ptePG->encID = current_encid();
	  newVal &= ~PG_RW;
	}
  }
  else {
	SVA_ASSERT (ptePG->encID == newPG->encID, 
	  "MMU: Mapping enclave page to wrong enclave container.");
  }

  /* =======================================================================
   * Deal with the enclave internal page(s). 
   * TODO: fix and test this case rigorously.
   * ======================================================================= */
  if (newPG->type == PG_ENC) {
	if (current_encid() != newPG->encID) {
	  PANIC ("SVA: MMU: Mapping enclave page to wrong enclave container.");
	}
  }

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
	if (origPG && isCodePg (origPG)) {
	  SVA_ASSERT ((*page_entry & PG_U), "Kernel attempting to modify code page mapping");
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
  unsigned long newPA = pageEntryToPA(mapping, /*is_to_frame=*/0);
  unsigned long newVA = (unsigned long) getVirtual(newPA);
  page_desc_t *newPG = getPageDescPtr(mapping);
  if(!newPG) return;

  /* If the new mapping is valid, update the counts for it. */
  if (mapping & PG_V) {
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

	/* 
	 * Update the reference count for the new page frame. Check that we aren't
	 * overflowing the counter.
	 */
	SVA_ASSERT (pgRefCount(newPG) < ((1u << 13) - 1), "MMU: overflow for the mapping count");
	newPG->count++;
  }
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
  if(!origPG) return;

  /* 
   * Only decrement the mapping count if the page has an existing valid
   * mapping.  Ensure that we don't drop the reference count below zero.
   */
  if ((mapping & PG_V) && origPG && (origPG->count)) {
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
  unsigned long origPA = pageEntryToPA(*pteptr, /*is_to_frame=*/0);
  unsigned long newPA = pageEntryToPA(mapping, /*is_to_frame=*/0);
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
  
  /* TODO: should we call tlb? kernel actually will do so. */
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
  page_entry_t * page_entry = get_pgeVaddr (vaddr, NULL);
  if (page_entry) {
	/*
	 * Chuqi: no need to do so as we already done this at declare.
	 *
	 * Make the direct map entry for the page read-only to ensure that the OS
	 * goes through SVA to make page table changes.  Also be sure to flush the
	 * TLBs for the direct map address to ensure that it's made read-only
	 * right away.
	 */
	// if (((*page_entry) & PG_PS) == 0) {
	//   // CLEANUP: Need to set to read-only anymore, since we use PKS now
	//   page_entry_store (page_entry, setMappingReadWrite(*page_entry));
	//   sva_mm_flush_tlb (vaddr);
	// }
  } else {
	/* debug log. unlikely to happen */
	log_err("Unfound page table entry for frame=0x%lx\n", frameAddr);
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
__update_mapping (unsigned long * pageEntryPtr, page_entry_t val) {
  /* 
   * If the given page update is valid then store the new value to the page
   * table entry, else raise an error.
   */
  switch (sm_validate_pt_update((page_entry_t *) pageEntryPtr, val)) {
	case 1:
	  /* Kernel thinks these should be RW, since it wants to write to them.
	   * Convert to read-only and carry on. 
	   * 
	   * Note: all writes to such objects should be made inside the SVA code.   
	   */
	  
	  /* ENCOS: TODO: enable this read-only after completing checks. */
	  // val = setMappingReadOnly (val);
	  __do_mmu_update ((page_entry_t *) pageEntryPtr, val);
	  break;

	case 2:
	  /* This updates are valid and we should allow them */
	  __do_mmu_update ((page_entry_t *) pageEntryPtr, val);
	  break;

	case 0:
	  /* 
	   * ENCOS: This updates junk kernel mappings (>>memSize). Not entirely
	   * clear why the kernel creates such mappings, but let's allow them.
	   */
	  __do_mmu_update ((page_entry_t *) pageEntryPtr, val);
	  break;

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
 * - vaddr
 *  0 - There is no mapping for this virtual address.
 *  Otherwise, a pointer to the PTE that controls the mapping of this virtual
 *  address is returned.
 * 
 * - level
 *  The level of the last page table entry found in the walk.
 *  1 - normal PTE (L1); 2 - hugepage PTE (L2).
 *  0 - no PTE found (no mapping).
 */
page_entry_t * 
get_pgeVaddr (unsigned long vaddr, int *level) {
  /* Pointer to the page table entry for the virtual address */
  page_entry_t *pge = 0;
  if (level)
	*level = 0;

  /* Get the base of the pml4 to traverse */
  unsigned long cr3 = (unsigned long) get_pagetable();
  if ((cr3 & 0xfffffffffffff000u) == 0)
	return 0;

  /* Get the VA of the pml4e for this vaddr */
  pgd_t *pgd = get_pgdVaddr ((unsigned char *)cr3, vaddr);  
  /* Had trouble with a NULL pointer dereference with pgd = 0; Switching to a fixed 4 level 
	 walk; Revert back later */
  if (pgd->pgd & PG_V) {
	LOG_WALK(3, "PGD (VA) 0x%lx (value: 0x%lx)\n", pgd, pgd->pgd);
	// if ((p4d->pgd.pgd & PG_V) > 0) {
	  /* Get the VA of the pdpte for this vaddr */
	  pud_t *pud = get_pudVaddr (pgd, vaddr);
	  if (pud->pud & PG_V) {
		/* 
		* The PDPE can be configurd in large page mode. If it is then we have the
		* entry corresponding to the given vaddr If not then we go deeper in the
		* page walk.
		*/
		LOG_WALK(3, "PUD (VA) 0x%lx (value: 0x%lx)\n", pud, pud->pud);
		if (pud->pud & PG_PS) {
		  pge = (page_entry_t*)&pud->pud;
		} else {
		  /* Get the pde associated with this vaddr */
		  pmd_t *pmd = get_pmdVaddr (pud, vaddr);

		  if (pmd->pmd & PG_V) {
			LOG_WALK(3, "PMD (VA) 0x%lx (value: 0x%lx)\n", pmd, pmd->pmd);
			/* 
			* As is the case with the pdpte, if the pde is configured for large
			* page size then we have the corresponding entry. Otherwise we need
			* to traverse one more level, which is the last. 
			*/
			if (pmd->pmd & PG_PS) {
			  pge = (page_entry_t*)&pmd->pmd;
			  if (level)
				*level = 2;
			} else {
			  pte_t *pte = get_pteVaddr (pmd, vaddr);
			  pge = (page_entry_t*)&pte->pte;
			  LOG_WALK(3, "PTE (VA) 0x%lx (value: 0x%lx)\n", pte, pte->pte);
			  if (level)
				*level = 1;
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
get_pgdVaddr (unsigned char * cr3, unsigned long vaddr) {
  /* Offset into the page table */
  unsigned long offset = (vaddr >> (39 - 3)) & vmask;
  return (pgd_t *) getVirtual (((unsigned long)cr3) | offset);
}

p4d_t *
get_p4dVaddr (pgd_t * pgd, unsigned long vaddr) {
  /* Offset into the page table */
//   unsigned long base   = (unsigned long)(pgd->pgd) & addrmask;
  unsigned long base   = pageEntryToPA(pgd->pgd, /*is_to_frame=*/0);
  unsigned long offset = (vaddr >> (39 - 3)) & vmask;
  return (p4d_t *) getVirtual (((unsigned long)base) | offset);
}

pud_t *
get_pudVaddr (pgd_t * pgd, unsigned long vaddr) {
  // #if defined(CONFIG_X86_5LEVEL)
	// unsigned long base   = (unsigned long) (p4d->p4d) & addrmask;
  // #else
	// unsigned long base   = (unsigned long) (pgd->pgd) & addrmask;
  unsigned long base   = pageEntryToPA(pgd->pgd, /*is_to_frame=*/0);
  // #endif
  unsigned long offset = (vaddr >> (30 - 3)) & vmask;
  return (pud_t *) getVirtual (base | offset);
}

pmd_t *
get_pmdVaddr (pud_t * pud, unsigned long vaddr) {
//   unsigned long base   = (unsigned long)(pud->pud) & addrmask;
  unsigned long base   = pageEntryToPA(pud->pud, /*is_to_frame=*/0);
  unsigned long offset = (vaddr >> (21 - 3)) & vmask;
  return (pmd_t *) getVirtual (base | offset);
}

pte_t *
get_pteVaddr (pmd_t * pmd, unsigned long vaddr) {
//   unsigned long base   = (unsigned long)(pmd->pmd) & addrmask;
  unsigned long base   = pageEntryToPA(pmd->pmd, /*is_to_frame=*/0);
  unsigned long offset = (vaddr >> (12 - 3)) & vmask;
  return (pte_t *) getVirtual (base | offset);
}

SECURE_WRAPPER(void, 
sva_stack_test, void) {
  unsigned long sp;
	asm volatile (
		"movq %%rsp, %0\n\t"
		: "=r" (sp)
		:
		: "memory"
	);
  log_info("sva_stack_test, SP=0x%lx.\n", sp);
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
   * performance numbers.
   *
   * ENCOS: Nested Kernel seems to not have implemented this. TODO.
   */

  /* read a PTE, and store it to simulate obtaining the load_cr3 mapping */
  /* ENCOS: "NULL" was added for compilation. TODO: check correctness */
  page_entry_t * page_entry = get_pgeVaddr (pg, NULL);
  page_entry_store(page_entry, *page_entry);
  sva_mm_flush_tlb (pg);

  /* simulate the branch */
  goto DOCHECK;
  
  //==-- Code residing on another set of pages --==//
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
  /* ENCOS: "NULL" was added for compilation. TODO: check correctness */
  page_entry = get_pgeVaddr (pg, NULL);
  page_entry_store(page_entry, *page_entry);
  sva_mm_flush_tlb (pg);

  MMULock_Release();
  return;
}

/*
 * Function: sva_write_cr0
 *
 * Description:
 *  SVA Intrinsic to write a value in cr0. We need to make sure write protection
 *  is enabled. 
 */
SECURE_WRAPPER(void,
sva_write_cr0, unsigned long val) {
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
SECURE_WRAPPER(void, sva_write_cr4, unsigned long val) {
  val |= (1ul << 24);    /* enable PKS */
  /* Chuqi: to enable SMAP/SMEP */
  // val |= (1 << 20);    /* enable SMEP */
  val |= (1 << 21);    /* enable SMAP */
  _load_cr4(val);
  MMULock_Release();
}

/*
 * Function: encos_write_msr
 *
 * Description:
 *  It is not necessary to wrap this with the secure call gate, given the
 *  security checks after wrmsr.
 */
// void
// sva_load_msr(u_int msr, uint64_t val) 
void encos_write_msrl(unsigned int msr, unsigned long val)
{

  if(msr == MSR_REG_EFER) {
	  val |= EFER_NXE;
  }
  _wrmsr(msr, val);
  /*
	* ENCOS: the outer kernel will always execute the below security checks.
	*/
  SVA_ASSERT(!((msr == MSR_REG_EFER) && !(val & EFER_NXE)),
		  "ENCOS: attempt to clear the EFER.NXE bit: %x.", val);
  SVA_ASSERT(msr != MSR_REG_PKRS, "ENCOS: OS attempts to write to PKRS.");
}

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
void 
declare_ptp_and_walk_pt_entries(unsigned long pageEntry, unsigned long
		numPgEntries, enum page_type_t pageLevel ) 
{ 
  int i;
  int traversedPTEAlready;
  enum page_type_t subLevelPgType;
  unsigned long numSubLevelPgEntries;
  page_desc_t *thisPg;
  page_entry_t pageMapping; 
  page_entry_t pageMapping_masked; 
  page_entry_t *pagePtr;

  /* Store the pte value for the page being traversed */
  pageMapping = (pageEntry);
  pageMapping_masked = pageEntryToPA(pageEntry, /*is_to_frame=*/0);
  if (pageMapping_masked > memSize) {
	LOG_WALK(1, "  \tJUNK (phys ==> %px\n)", pageMapping_masked);
	return;
  }

  /* Set the page pointer for the given page */
  pagePtr = (page_entry_t*) getVirtual((unsigned long)(pageMapping_masked));

  /* Get the page_desc for this page */
  thisPg = getPageDescPtr(pageMapping_masked);
  if (!thisPg) return;

  /* Mark if we have seen this traversal already */
  traversedPTEAlready = (thisPg->type != PG_UNUSED);

  /* Set the virtual address of the page (for later use) */
  thisPg->pgVaddr = pagePtr;

  /* Character inputs to make the printing pretty for debugging */
  char * indent = "";
  char * l5s = "L5:";
  char * l4s = "\tL4:";
  char * l3s = "\t\tL3:";
  char * l2s = "\t\t\tL2:";
  char * l1s = "\t\t\t\tL1:";

  switch (pageLevel){
#if defined(CONFIG_X86_5LEVEL)
	case PG_L5:
		indent = l5s;
		LOG_WALK(2, "%sSetting L5 Page: ptr: %px, mapping:0x%lx, mapping (masked):0x%lx\n", 
			indent, pagePtr, pageMapping, pageMapping_masked);
		break;
#endif
	case PG_L4:
		indent = l4s;
		LOG_WALK(2, "%sSetting L4 Page: ptr: %px, mapping:0x%lx, mapping (masked):0x%lx\n", indent, pagePtr, 
			pageMapping, pageMapping_masked);
		break;
	case PG_L3:
		indent = l3s;
		LOG_WALK(2, "%sSetting L3 Page: ptr: %px mapping:0x%lx, mapping (masked):0x%lx\n", indent, pagePtr, 
			pageMapping, pageMapping_masked);
		break;
	case PG_L2:
		indent = l2s;
		LOG_WALK(2, "%sSetting L2 Page: ptr: %px, mapping:0x%lx, mapping (masked):0x%lx\n", indent, pagePtr, 
			pageMapping, pageMapping_masked);
		break;
	case PG_L1:
		indent = l1s;
		LOG_WALK(2, "%sSetting L1 Page: ptr: %px, mapping:0x%lx, mapping (masked):0x%lx\n", indent, pagePtr, 
			pageMapping, pageMapping_masked);
		break;
	default:
		break;
  }

  /*
   * For each level of page we do the following:
   *  - Set the page descriptor type for this page table page
   *  - Set the sub level page type and the number of entries for the
   *    recursive call to the function.
   */
  switch(pageLevel){

	case PG_L5:
	  thisPg->type = PG_L5;       /* Set the page type to L4 */
	  ++(thisPg->count);
	  subLevelPgType = PG_L4;
	  numSubLevelPgEntries = NPGDEPG;//    numPgEntries;
	  break;

	case PG_L4:
	  thisPg->type = PG_L4;       /* Set the page type to L4 */
	  ++(thisPg->count);
	  subLevelPgType = PG_L3;
	  numSubLevelPgEntries = NP4DEPG;//    numPgEntries;
	  break;

	case PG_L3:

	  if ((pageMapping & PG_PS) != 0) {
		LOG_WALK(1, "\tIdentified 1GB page...\n");
		unsigned long index = (pageMapping_masked & ~PUDMASK) / pageSize;
		page_desc[index].type = PG_KDATA;
		++(page_desc[index].count);
		return;
	  } else {

		/* TODO: Determine why we want to reassign an L4 to an L3 */
		if (thisPg->type != PG_L4)
		  thisPg->type = PG_L3;       /* Set the page type to L3 */
		++(thisPg->count);
		subLevelPgType = PG_L2;
		numSubLevelPgEntries = NPUDEPG; //numPgEntries;
	  }
	  break;

	case PG_L2:
	  /* 
	   * If my L2 page mapping signifies that this mapping references a 2MB
	   * page frame, then get the frame address using the correct page mask
	   * for a L3 page entry and initialize the page_desc for this entry.
	   * Then return as we don't need to traverse frame pages.
	   */
	  if ((pageMapping & PG_PS) != 0) {
		LOG_WALK(1, "\tIdentified 2MB page (L2_PTE: 0x%lx, mapping:0x%lx, VA: 0x%lx)...\n",
				  pageMapping, pageMapping_masked, __va(pageMapping_masked));

		/* ENCOS: check. */
		// unsigned long index = (pageMapping & ~PUDMASK) / pageSize;
		unsigned long index = (pageMapping_masked & ~PMDMASK) / pageSize;
		page_desc[index].type = PG_KDATA;
		++(page_desc[index].count);
		return;
	  } else {
		thisPg->type = PG_L2;         /* Set the page type to L2 */
		++(thisPg->count);
		subLevelPgType = PG_L1;
		numSubLevelPgEntries = NPMDPG; // numPgEntries;
	  }
	  break;

	case PG_L1:
	  thisPg->type = PG_L1;           /* Set the page type to L1 */
	  ++(thisPg->count);
	  subLevelPgType = PG_KDATA;
	  numSubLevelPgEntries = NPTEPG;    // numPgEntries;
	  break;

	default:
	  PANIC("SVA: walked an entry with invalid page type.");
  }
  
  /* 
   * There is one recursive mapping, which is the last entry in the PML4 page
   * table page. Thus we return before traversing the descriptor again.
   * Notice though that we keep the last assignment to the page as the page
   * type information. 
   */
  if(traversedPTEAlready) {
	LOG_WALK(2, "%sRecursed on already initialized page_desc\n", indent);
	return;
  }

  /* Statistics and debugging */
  u_long nNonValPgs=0;
  u_long nValPgs=0;

  /* 
   * Iterate through all the entries of this page, recursively calling the
   * walk on all sub entries.
   */
  bool skipAssert = false;
  for (i = 0; i < numSubLevelPgEntries; i++){
	/* ENCOS: Left this here from the NK implementation. I don't think this is required. */
#if 0
	/*
	 * Do not process any entries that implement the direct map.  This prevents
	 * us from marking physical pages in the direct map as kernel data pages.
	 */
	if ((pageLevel == PG_L4) && (i == (0xffff888000000000 / 0x1000))) {
	  continue;
	}
#endif

	page_entry_t * nextEntry = & pagePtr[i];
	/* 
	 * ENCOS: These are reserved mappings and they crash the system if you access.
	 * TODO: Figure out a better trick (e.g., kernel notifies of these regions).
	 */
	if ((nextEntry >= 0xffff8880fec00000) && (nextEntry <= 0xffff8880fef00000)) {
	  skipAssert = true;
	  continue;
	}

	LOG_WALK(5, "%sPagePtr in loop: %px, val: 0x%lx\n", indent, nextEntry, *nextEntry);

	/* 
	 * If this entry is valid then recurse the page pointed to by this page
	 * table entry.
	 */
	if (*nextEntry & PG_V) {
	  nValPgs++;
	  /* 
	  * If we hit the level 1 pages we have hit our boundary condition for
	  * the recursive page table traversals. Now we just mark the leaf page
	  * descriptors.
	  */        
	  if (pageLevel == PG_L1){
		LOG_WALK(5, "%sInitializing leaf entry: pteaddr: %px, mapping: 0x%lx\n",
				  indent, nextEntry, *nextEntry);
	  } else {
		LOG_WALK(5, "%sProcessing:pte addr: %px, newPgAddr: %p, mapping: 0x%lx\n",
			  indent, nextEntry, (*nextEntry & PG_FRAME), *nextEntry );
		  declare_ptp_and_walk_pt_entries((unsigned long)*nextEntry,
				  numSubLevelPgEntries, subLevelPgType); 
	  }
	}
	else {
	  nNonValPgs++;
	} 
  }

out:
  LOG_WALK(2, "%sThe number of || non valid pages: %lu || valid pages: %lu\n",
			  indent, nNonValPgs, nValPgs);
  if (!skipAssert)
	SVA_ASSERT((nNonValPgs + nValPgs) == 512, "Wrong number of entries traversed");
}

// isPTP check
void 
ptp_check(unsigned long pageEntryPA, unsigned long
		numPgEntries, enum page_type_t pageLevel, unsigned long target_page_nr, int *isPTP) 
{ 
  int i;
  enum page_type_t subLevelPgType;
  unsigned long numSubLevelPgEntries;
  page_entry_t pageMapping; 
  page_entry_t pageMapping_masked; 
  page_entry_t *pagePtr;
  unsigned long nx_mask    = ~(1 << 63);
  unsigned long cbit_mask  = ~(1 << 51);

  // printk("PTP Check Debug: PA: %lx, Level: %d\n", pageEntryPA, pageLevel);

  unsigned long page_nr_mask = 0x0000000FFFFFF000;
  if(((pageEntryPA & page_nr_mask) >> 12)  == target_page_nr)
	*isPTP = 1;

  // return is check found
  if (*isPTP == 1)
	return;

  /* Store the pte value for the page being traversed */
  pageMapping = (pageEntryPA);
  pageMapping_masked = (((pageEntryPA & PG_FRAME) & nx_mask) & cbit_mask);
  if (pageMapping_masked > memSize) {
	LOG_WALK(1, "  \tJUNK (phys ==> %px\n)", pageMapping_masked);
	return;
  }

  /* Set the page pointer for the given page */
  pagePtr = (page_entry_t*) getVirtual((unsigned long)(pageMapping_masked));

  /*
   * For each level of page we do the following:
   *  - Set the page descriptor type for this page table page
   *  - Set the sub level page type and the number of entries for the
   *    recursive call to the function.
   */
  switch(pageLevel){
	case PG_L4: 
	  subLevelPgType = PG_L3;
	  numSubLevelPgEntries = NP4DEPG;//    numPgEntries;
	  break;

	case PG_L3:

	  if ((pageMapping & PG_PS) != 0) {
		return;
	  } else {
		subLevelPgType = PG_L2;
		numSubLevelPgEntries = NPUDEPG; //numPgEntries;
	  }
	  break;

	case PG_L2:
	  /* 
	   * If my L2 page mapping signifies that this mapping references a 2MB
	   * page frame, then get the frame address using the correct page mask
	   * for a L3 page entry and initialize the page_desc for this entry.
	   * Then return as we don't need to traverse frame pages.
	   */
	  if ((pageMapping & PG_PS) != 0) {
		return;
	  } else {
		subLevelPgType = PG_L1;
		numSubLevelPgEntries = NPMDPG; // numPgEntries;
	  }
	  break;

	case PG_L1:
	  subLevelPgType = PG_KDATA;
	  numSubLevelPgEntries = NPTEPG;    // numPgEntries;
	  break;

	default:
	  PANIC("SVA: walked an entry with invalid page type.");
  }

  /* Statistics and debugging */
  u_long nNonValPgs=0;
  u_long nValPgs=0;

  /* 
   * Iterate through all the entries of this page, recursively calling the
   * walk on all sub entries.
   */
  bool skipAssert = false;
  for (i = 0; i < numSubLevelPgEntries; i++){
	/* ENCOS: Left this here from the NK implementation. I don't think this is required. */

	page_entry_t * nextEntry = & pagePtr[i];
	/* 
	 * ENCOS: These are reserved mappings and they crash the system if you access.
	 * TODO: Figure out a better trick (e.g., kernel notifies of these regions).
	 */
	if ((nextEntry >= 0xffff8880fec00000) && (nextEntry <= 0xffff8880fef00000)) {
	  skipAssert = true;
	  continue;
	}

	/* 
	 * If this entry is valid then recurse the page pointed to by this page
	 * table entry.
	 */
	if (*nextEntry & PG_V) {
	  nValPgs++;
	  /* 
	  * If we hit the level 1 pages we have hit our boundary condition for
	  * the recursive page table traversals. Now we just mark the leaf page
	  * descriptors.
	  */        
	  if (pageLevel == PG_L1){

	  } else {
		  ptp_check((unsigned long)*nextEntry,
				  numSubLevelPgEntries, subLevelPgType, target_page_nr, isPTP); 
	  }
	}
	else {
	  nNonValPgs++;
	} 
  }

out:
  // LOG_WALK(2, "%sThe number of || non valid pages: %lu || valid pages: %lu\n",
  //             indent, nNonValPgs, nValPgs);
  if (!skipAssert)
	SVA_ASSERT((nNonValPgs + nValPgs) == 512, "Wrong number of entries traversed");
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
init_protected_pages (unsigned long startVA, unsigned long endVA, enum page_type_t type) {
  unsigned long page;
  for (page = startVA; page < endVA; page += pageSize) {
	  /* Set the physical page descriptor with the pgType */
	  page_desc_t *pgDesc = getPageDescPtr(__pa(page));
	  if (!pgDesc) {
		/* Panic if this page does not have a descriptor */
		PANIC ("[WARNING] Page (%px) does not have a descriptor! \n", page);
	  }
	  pgDesc->type = type;

	  /* ENCOS: Set WP/PKS for the virtual address. */
	  set_page_protection(page, /*should_protect=*/1);
  }
}

/*
 * Function: makePTReadOnly()
 *
 * Description:
 *  Scan through all of the page descriptors and find all the page descriptors
 *  for page table pages.  For each such page, make the virtual page that maps
 *  it into the direct map read-only.
 */
static inline void
makePTReadOnly (void) {
  /* Disable page protection */
  unprotect_paging();

  /*
   * For each physical page, determine if it is used as a page table page.
   * If so, make its entry in the direct map read-only.
   */
  unsigned long paddr;
  for (paddr = 0; paddr < memSize; paddr += pageSize) {
	enum page_type_t pgType = getPageDescPtr(paddr)->type;

	if ((PG_L1 <= pgType) && (pgType <= PG_L4)) {
	  #if 0
	  page_entry_t * pageEntry = get_pgeVaddr (getVirtual(paddr), NULL);
	  if (!pageEntry) {
		LOG_PRINTK("  [warning] empty virtual address for %px\n", paddr);
		continue;
	  }
	  #endif

	  page_entry_t * pageEntry = getPageDescPtr(paddr)->pgVaddr;
	  // LOG_PRINTK("Page Entry ==> %px\n", pageEntry);

	  /* 
	   * ENCOS: These are reserved mappings and they crash the system if you access.
	   * Figure out a better trick later.
	   */
	  if ((pageEntry >= 0xffff8880fec00000) && (pageEntry <= 0xffff8880fef00000))
		continue;

	  // Don't make direct map entries of larger sizes read-only,
	  // they're likely to be broken into smaller pieces later
	  // which is a process we don't handle precisely yet.
	  if (((*pageEntry) & PG_PS) == 0) {
		  /* 
		   * ENCOS: As soon as I enable this, the kernel hangs. This is likely 
		   * because you don't currently set protections (i.e., CR0.WP) disable.
		   *
		   * IMPORTANT FIX. 
		   */
		  // page_entry_store (pageEntry, setMappingReadOnly(*pageEntry));
		  page_entry_store (pageEntry, *pageEntry);
	  }
	}

  }

  /* Re-enable page protection */
  protect_paging();
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
SECURE_WRAPPER(void, sva_mmu_init, void) {
  init_MMULock();
  MMULock_Acquire();

  /* Hello World! */
  LOG_PRINTK("[.] Initializing: SECURE memory management unit \n");

  /* Initialize page descriptor array */
  memset(page_desc, 0, numPageDescEntries * sizeof(page_desc_t));

  /* Protect the kernel text (code) region */
  // LOG_PRINTK("_stext: %lx, _etext: %lx\n", _stext, _etext);
  // init_protected_pages((unsigned long) PFN_ALIGN(_stext), (unsigned long) PFN_ALIGN(_etext), 
	// PG_KCODE);
  
  /* Protect the SVA data and code pages */
  LOG_PRINTK("_svastart va=0x%lx pa=0x%lx, _svaend va=0x%lx pa=0x%lx\n", 
			  _svastart, __pa(_svastart), _svaend, __pa(_svaend));
  init_protected_pages((unsigned long) _svastart, (unsigned long) _svaend, PG_SVA);

  LOG_PRINTK("__svamem_text_start va=0x%lx pa=0x%lx, __svamem_text_end va=0x%lx pa=0x%lx\n", 
			__svamem_text_start, __pa(__svamem_text_start), __svamem_text_end, __pa(__svamem_text_end));
  init_protected_pages((unsigned long)__svamem_text_start, (unsigned long)__svamem_text_end, PG_SVA);

  LOG_PRINTK("__svamem_priv_text_start va=0x%lx pa=0x%lx, __svamem_priv_text_end va=0x%lx pa=0x%lx\n", 
			__svamem_priv_text_start, __pa(__svamem_priv_text_start), __svamem_priv_text_end, __pa(__svamem_priv_text_end));

  /* Walk the kernel page tables and initialize the sva page_desc */
  unsigned long kpgdPA = (sva_get_current_pgd() << 12);
  printk("sva_mmu_init KPGDPA: %lx\n", kpgdPA);
  
  declare_ptp_and_walk_pt_entries(kpgdPA, NP4DEPG, PG_L4);
  
  /* Now load the initial value of the cr3 to complete kernel init */
  /* ENCOS: Complete. */
  // _load_cr3(kpgdMapping->pgd & PG_FRAME);

  /* Make existing page table pages read-only */
  // Rahul (TODO): why does this break with the PTE flag changes in pks.c page split ??
  // makePTReadOnly(); 

  /*
   * Note that the MMU is now initialized.
   */
  mmuIsInitialized = 1;

  LOG_PRINTK("[*] SECURE memory management unit initialized \n");

  MMULock_Release();
}

/* 
 * During MMU update, if the PTE is not declared as the correct type,
 * we re-declare it here.
 */
void declare_internal(unsigned long frameAddr, int level) {
  /* Get the page_desc for the page frame */
  page_desc_t *pgDesc = getPageDescPtr(frameAddr);
  if(!pgDesc) return;

  /* Validate the re-declared page is not a sensitive page */
  SVA_ASSERT(!isSensitivePg(pgDesc), 
		"Trying to redeclare a sensitive page PA=0x%lx (type=0x%lx) as a level %d PTP.\n",
		frameAddr, pgDesc->type, level);
  
  /* Reset the whole page */
  memset((void*) pgDesc, 0, sizeof(page_desc_t));

  /* Setup metadata tracking for this new page */
  pgDesc->type = level;

  set_page_protection((unsigned long)__va(frameAddr), /*should_protect=*/1);

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
sva_declare_l1_page, unsigned long frameAddr) {
  MMULock_Acquire();

  /* Get the page_desc for the newly declared l4 page frame */
  page_desc_t *pgDesc = getPageDescPtr(frameAddr);
  if(!pgDesc) return;

  /*
   * Make sure that this is already an L1 page, an unused page, or a kernel
   * data page.
   */
  switch (pgDesc->type) {
	case PG_UNUSED:
	case PG_L1:
	case PG_KDATA:
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

	set_page_protection((unsigned long)__va(frameAddr), /*should_protect=*/1);

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

  LOG_DECLARE("Declaring L2 page (%px)\n", (void*) frameAddr);

  /* Get the page_desc for the newly declared l2 page frame */
  page_desc_t *pgDesc = getPageDescPtr(frameAddr);
  if(!pgDesc) return;

  /*
   * Make sure that this is already an L2 page, an unused page, or a kernel
   * data page.
   */
  switch (pgDesc->type) {
	case PG_UNUSED:
	case PG_L2:
	case PG_KDATA:
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

	set_page_protection((unsigned long)__va(frameAddr), /*should_protect=*/1);

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
sva_declare_l3_page, unsigned long frameAddr) {
  MMULock_Acquire();

  // printk("ENCOS-Internal: Declaring L3 page internally. (frameaddr = %px)\n", 
  //   (void*) frameAddr);  
  LOG_DECLARE("Declaring L3 page (%px)\n", (void*) frameAddr);

  /* Get the page_desc for the newly declared l4 page frame */
  page_desc_t *pgDesc = getPageDescPtr(frameAddr);
  if(!pgDesc) return;

  /*
   * Make sure that this is already an L3 page, an unused page, or a kernel
   * data page.
   */
  switch (pgDesc->type) {
	case PG_UNUSED:
	case PG_L3:
	case PG_KDATA:
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
	set_page_protection((unsigned long)__va(frameAddr), /*should_protect=*/1);

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
sva_declare_l4_page, unsigned long frameAddr) {
  MMULock_Acquire();

  // printk("ENCOS-Internal: Declaring L4 page internally. (frameaddr = %px)\n", 
  //   (void*) frameAddr);
  LOG_DECLARE("Declaring L4 page (%px)\n", (void*) frameAddr);

  /* Get the page_desc for the newly declared l4 page frame */
  page_desc_t *pgDesc = getPageDescPtr(frameAddr);
  if(!pgDesc) return;

  /*
   * Make sure that this is already an L4 page, an unused page, or a kernel
   * data page.
   */
  switch (pgDesc->type) {
	case PG_UNUSED:
	case PG_L4:
	case PG_KDATA:
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

	/*
	 * Reset the virtual address which can point to this page table page.
	 */
	pgDesc->pgVaddr = 0;
	set_page_protection((unsigned long)__va(frameAddr), /*should_protect=*/1);
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
sva_declare_l5_page, unsigned long frameAddr) {
  MMULock_Acquire();

  // printk("ENCOS-Internal: Declaring L5 page internally. (frameaddr = %px)\n", 
  //   (void*) frameAddr);  
  LOG_DECLARE("Declaring L5 page (%px)\n", (void*) frameAddr);

  /* Get the page_desc for the newly declared l4 page frame */
  page_desc_t *pgDesc = getPageDescPtr(frameAddr);
  if(!pgDesc) return;

  /*
   * Make sure that this is already an L4 page, an unused page, or a kernel
   * data page.
   */
  switch (pgDesc->type) {
	case PG_UNUSED:
	case PG_L5:
	case PG_KDATA:
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
	// Chuqi: ignore, we don't use L5 paging anyways.
	// set_page_protection((unsigned long)__va(frameAddr), /*should_protect=*/1);

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
sva_remove_page, unsigned long paddr) {
  MMULock_Acquire();

  /* Get the page_desc for the page frame */
  page_desc_t *pgDesc = getPageDescPtr(paddr);
  if(!pgDesc) {
	MMULock_Release();
	return;
  }
  // set_page_protection((unsigned long)__va(paddr), /*should_protect=*/0);
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
	  /* 
	   * ENCOS: (IMPORTANT) Please check this case out. Why are we getting undeclared pages and
	   * we get many of them if I uncomment these lines.
	   */
	  
	  // if (mmuIsInitialized) {
	  //   LOG_PRINTK (" (warning) undeclare bad page type: %lx %lx\n", paddr, pgDesc->type);
	  // }
	  MMULock_Release();
	  return;
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
  // TODO: check
  if ((pgDesc->count == 1) || (pgDesc->count == 0)) {
	/*
	 * Mark the page frame as an unused page.
	 */
	set_page_protection((unsigned long)__va(paddr), /*should_protect=*/0);
	pgDesc->type = PG_UNUSED;
	/* Reset the page descriptor entry */
	memset(pgDesc, 0, sizeof(page_desc_t));
  } else {
	panic ("SVA: remove_page: type=%x count %x\n", pgDesc->type, pgDesc->count);
  }
 
  MMULock_Release();
  return;
}

SECURE_WRAPPER(void,
sva_remove_pages, unsigned long paddr, unsigned int order) {
  int nr_pages = 1 << order;
  for (int i = 0; i < nr_pages; i++) {
	sva_remove_page_secure(paddr + i * pageSize);
  }
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
SECURE_WRAPPER(int,
sva_update_l1_mapping, pte_t *pte, page_entry_t val) {
  MMULock_Acquire();

  /* Debugging */
  LOG_UPDATE("Updating L1 page (%px, %lx)", pte, val);

  /*
   * Ensure that the PTE pointer points to an L1 page table.  If it does not,
   * then report an error.
   */
  void *p = pte;
  page_desc_t * ptDesc = getPageDescPtr (__pa(pte));
	if(!ptDesc) {
	  MMULock_Release();
	  return -1;
	}

  /* if it is undeclared before */
  if(ptDesc->type != PG_L1) {
	declare_internal(__pa(pte), 1);
  }
  /*
   * Update the page table with the new mapping.
   */
  __update_mapping(&pte->pte, val);

  MMULock_Release();
  return 0;
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

  /* Debugging */
  LOG_UPDATE("Updating L2 page (%px,%lx)", pmd, val);

  /*
   * Ensure that the PTE pointer points to an L2 page table.  If it does not,
   * then report an error.
   */

  page_desc_t * ptDesc = getPageDescPtr (__pa(pmd));

  /* TODO: junk mapping, just let it go */
  if(!ptDesc) {
	LOG_PRINTK("Found junk mapping pmdVA=0x%lx val=0x%lx.\n", (unsigned long)pmd, val);
	pmd->pmd = (pmdval_t) val;
	MMULock_Release();
	return;
  }

  if(ptDesc->type != PG_L2) {
	declare_internal(__pa(pmd), 2);
  }

  /*
   * Update the page mapping.
   */
  __update_mapping(&pmd->pmd, val);
	
  // int isPTP = 0;
  // unsigned long page_nr_mask = 0x0000000FFFFFF000;
  // unsigned long page_nr = (__pa(pmd) & page_nr_mask) >> 12;
	// ptp_check(read_cr3(), 512, PG_L4, page_nr, &isPTP);
  // printk("After L2 __update_mapping (pmd_pa: 0x%lx; page_type: %d), ptp_check pmd result: %d\n", 
  //         __pa(pmd), ptDesc->type, isPTP);

  // panic("GGWP PTP_CHECK\n");

  MMULock_Release();
  return;
}

/*
 * Updates a level3 mapping 
 */
SECURE_WRAPPER(void, sva_update_l3_mapping, pud_t * pud, page_entry_t val) {
  MMULock_Acquire();

  /* Debugging */
  LOG_UPDATE("Updating L3 page (%px, %lx)", pud, val);

  /*
   * Ensure that the PTE pointer points to an L3 page table.  If it does not,
   * then report an error.
   */
  page_desc_t * ptDesc = getPageDescPtr (__pa(&pud->pud));
  if(!ptDesc) {
	MMULock_Release();
	return;
  }

  if(ptDesc->type != PG_L3) {
	/* TODO: Shouldn't we set this protection bit? */
	declare_internal(__pa(pud), 3);
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

  /* Debugging */
  LOG_UPDATE("Updating L4 page (%px, %lx)", p4d, val);

  /*
   * Ensure that the PTE pointer points to an L4 page table.  If it does not,
   * then report an error.
   */
  #if defined(CONFIG_X86_5LEVEL)
	page_desc_t * ptDesc = getPageDescPtr (__pa(&p4d->p4d));
  #else
	page_desc_t * ptDesc = getPageDescPtr (__pa(&p4d->pgd.pgd));
  #endif
  if(!ptDesc) {
	MMULock_Release();
	return;
  }

  if(ptDesc->type != PG_L4) {
	declare_internal(__pa(p4d), 4);
  }

  #if defined(CONFIG_X86_5LEVEL)
	__update_mapping(&p4d->p4d, val);
  #else
	__update_mapping(&p4d->pgd.pgd, val);
  #endif

  MMULock_Release();
  return;
}

/*
 * Updates a level5 mapping 
 */
SECURE_WRAPPER( void, sva_update_l5_mapping, pgd_t * pgd, page_entry_t val) {
  MMULock_Acquire();

  /* Debugging */
  LOG_UPDATE("Updating L5 page (%px, %lx)", pgd, val);

  /*
   * Ensure that the PTE pointer points to an L5 page table.  If it does not,
   * then report an error.
   */  
  page_desc_t * ptDesc = getPageDescPtr (__pa(&pgd->pgd));
  if(!ptDesc) {
	MMULock_Release();
	return;
  }

  if(!isL5Pg(ptDesc)) {
	declare_internal(__pa(pgd), 5);      
  }

  __update_mapping(&pgd->pgd, val);

  MMULock_Release();
  return;
}

/* 
 * Secure text poking (for kernel relocations)
 */
typedef void text_poke_f(void *dst, const void *src, size_t len);
struct sva_secure_poke_t {
  page_entry_t pte;
  page_entry_t ptetwo;
  pte_t *ptep;
  bool cross_page_boundary;
};

SECURE_WRAPPER(void, sva_secure_poke, 
  text_poke_f func, void *addr, const void *src, size_t len,
  struct sva_secure_poke_t* spt) 
{
  /* Debugging */
  // LOG_PRINTK("sva_secure_poke (%px, %px, %px, %lx, %px)\n",
  //   func, addr, src, len, spt);

	// set_pte_at(poking_mm, poking_addr, ptep, pte);
	// set_pte_at(spt->poking_mm, poking_addr + PAGE_SIZE, spt->ptep + 1, spt->ptetwo);
  __do_mmu_update(spt->ptep, spt->pte);
	if (spt->cross_page_boundary) {
		__do_mmu_update(spt->ptep + 1, spt->ptetwo);
	}

	/*
	 * Loading the temporary mm behaves as a compiler barrier, which
	 * guarantees that the PTE will be set at the time memcpy() is done.
	 */
  barrier();

	kasan_disable_current();
	func((u8 *)poking_addr + offset_in_page(addr), src, len);
	kasan_enable_current();

	/*
	 * Ensure that the PTE is only cleared after the instructions of memcpy
	 * were issued by using a compiler barrier.
	 */
	barrier();

	// pte_clear(poking_mm, poking_addr, ptep);
  // 	pte_clear(poking_mm, poking_addr + PAGE_SIZE, ptep + 1);
  __do_mmu_update(spt->ptep, 0);
	if (spt->cross_page_boundary)
	__do_mmu_update(spt->ptep + 1, 0);
	
  return;
}

SECURE_WRAPPER(void, sva_clear_page, 
  void *page) {
  // if(mmuIsInitialized)
	// panic("Calling sva_clear_page post system boot\n");
  
  // ENCOS (TODO): Check if this is an PTP page, and use the __update_mapping function to update the 
  // pageDesc count, pgVaddr etc.
  // ENCOS (TODO): Disallow setting this page's contents to zero, if it is a PG_ENC page (But will have to allow upon an enclave exit/cleanup)
  memset(page, 0, PAGE_SIZE);
	
  return;
}

// Inline function to read and return the EFLAGS register
static inline unsigned long read_eflags(void) {
    unsigned long eflags;
    asm volatile (
        "pushf\n\t"        // Push EFLAGS onto the stack
        "pop %0"           // Pop the top of the stack into the variable
        : "=r" (eflags)    // Output operand
        :                  // No input operands
        : "memory"         // Clobber list
    );
    return eflags;
}

SECURE_WRAPPER(unsigned long, 
sva_copy_user_generic, 
void *to, const void *from, unsigned long len) {
  
//   // printk("CR4.SMAP = %d\n", (__read_cr4() >> 21) & 0x1); // SMAP is enabled
//   // printk("EFLAGS: 0x%lx\n", read_eflags());
//   stac();  
//   // asm volatile(__ASM_STAC);
//   /*
//    * If CPU has FSRM feature, use 'rep movs'.
//    * Otherwise, use rep_movs_alternative.
//    */
//   asm volatile(
//     "1:\n\t"
//     ALTERNATIVE("rep movsb",
//           "call rep_movs_alternative", ALT_NOT(X86_FEATURE_FSRM))
//     "2:\n"
//     _ASM_EXTABLE_UA(1b, 2b)
//     :"+c" (len), "+D" (to), "+S" (from), ASM_CALL_CONSTRAINT
//     : : "memory", "rax");
  
//   // asm volatile(__ASM_CLAC);
//   clac();

  return len;
}
