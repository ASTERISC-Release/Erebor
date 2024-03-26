/*===- x86.h - SVA Execution Engine ----------------------------------------===
 *
 *                        Secure Virtual Architecture
 *
 * This file was developed by the LLVM research group and is distributed under
 * the University of Illinois Open Source License. See LICENSE.TXT for details.
 *
 *===----------------------------------------------------------------------===
 *
 * This file defines structures used by the x86_64 architecture.
 *
 *===----------------------------------------------------------------------===
 */

// #include <sys/types.h>

#ifndef _SVA_X86_H
#define _SVA_X86_H

/* Flags bits in x86_64 PTE entries */
static const unsigned PTE_PRESENT  = 0x0001u;
static const unsigned PTE_CANWRITE = 0x0002u;
static const unsigned PTE_CANUSER  = 0x0004u;
static const unsigned PTE_PS       = 0x0080u;
#endif
