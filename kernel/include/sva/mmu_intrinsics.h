/*===- mmu_intrinsics.h - SVA Execution Engine  =------------------------------------===
 * 
 *                        Secure Virtual Architecture
 *
 * This file was developed by the LLVM research group and is distributed under
 * the University of Illinois Open Source License. See LICENSE.TXT for details.
 * 
 *
 *===----------------------------------------------------------------------===
 *
 *       Filename:  mmu_intrinsics.h
 *
 *    Description:  This file exports the sva instrinsics available for
 *                  manipulating page tables. The key reason to have this in
 *                  addition to the mmu.h is that the mmu.h code is primarily
 *                  internal SVA functionality for the mmu management and
 *                  should not be exported. 
 *
 *        Version:  1.0
 *        Created:  04/24/13 04:31:31
 *       Revision:  none
 *
 *===----------------------------------------------------------------------===
 */

/*-
 * Copyright (c) 2003 Peter Wemm.
 * Copyright (c) 1993 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: release/9.0.0/sys/amd64/include/cpufunc.h 223796 2011-07-05 18:42:10Z jkim $
 */

#ifndef SVA_MMU_INTRINSICS_H
#define SVA_MMU_INTRINSICS_H

#include "mmu_types.h"
#include "svamem.h"
#include "msr.h"

/*
 *****************************************************************************
 * SVA intrinsics implemented in the library
 *****************************************************************************
 */
extern void sva_stack_test (void);
extern void bench_encos_write_msrl(unsigned int msr, unsigned long val);
extern void sva_mm_load_pgtable (void * pg);
extern void sva_declare_l1_page (uintptr_t frame);
extern void sva_declare_l2_page (uintptr_t frame);
extern void sva_declare_l3_page (uintptr_t frame);
extern void sva_declare_l4_page (uintptr_t frame);
extern void sva_declare_l5_page (uintptr_t frame);
extern void sva_remove_page     (uintptr_t frame);
extern void sva_remove_pages    (uintptr_t frame, unsigned int order);

extern int sva_update_l1_mapping (pte_t * ptePtr, page_entry_t val);
extern void sva_update_l2_mapping (pmd_t * pmdPtr, page_entry_t val);
extern void sva_update_l3_mapping (pud_t * pudPtr, page_entry_t val);
extern void sva_update_l4_mapping (p4d_t * p4dPtr, page_entry_t val);
extern void sva_update_l5_mapping (pgd_t * pgdPtr, page_entry_t val);
extern void sva_remove_mapping (page_entry_t * ptePtr);
extern void sva_mmu_init(void);
extern void sva_clear_page(void* page);
extern unsigned long sva_copy_user_generic(void* to, const void* from, unsigned long len);
extern void sva_memcpy (void *dst, void *src, unsigned long len);

extern void SVATEXT sm_entry_map_priv_page(void);
extern void SVATEXT sm_exit_unmap_priv_page(void);

/* Key initialization and secure storage allocation */
extern void * sva_translate(void * entryPoint);

/*
 *****************************************************************************
 * SVA intrinsics implemented as inline functions
 *****************************************************************************
 */

/*
 * Function: sva_mm_flush_tlb()
 *
 * Description:
 *  Flush all TLB's holding translations for the specified virtual address.
 *
 * Notes:
 *  I had to look at the FreeBSD implementation of invlpg() to figure out that
 *  you need to "dereference" the address to get the operand to the inline asm
 *  constraint to work properly.  While perhaps not necessary (because I don't
 *  think such a trivial thing can by copyrighted), the fact that I referenced
 *  the FreeBSD code is why we have the BSD copyright and attribute comment at
 *  the top of this file.
 */
static inline void
sva_mm_flush_tlb (void * address) {
  __asm__ __volatile__ ("invlpg %0" : : "m" (*((char *)address)) : "memory");
  return;
}

#endif /* SVA_MMU_INTRINSICS_H */
