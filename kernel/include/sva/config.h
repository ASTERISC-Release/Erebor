/*===- config.h - SVA Utilities --------------------------------------------===
 * 
 *                        Secure Virtual Architecture
 *
 * This file was developed by the LLVM research group and is distributed under
 * the University of Illinois Open Source License. See LICENSE.TXT for details.
 * 
 *===----------------------------------------------------------------------===
 *
 * This header file contains macros that can be used to configure the SVA
 * Execution Engine.
 *
 *===----------------------------------------------------------------------===
 */

#ifndef _SVA_CONFIG_H
#define _SVA_CONFIG_H

/* Determine whether the virtual ghost features are enabled */
#ifdef VG
static const unsigned char vg = 1;
#else
static const unsigned char vg = 0;
#endif

/* Total number of processors supported by this SVA Execution Engine */
static const unsigned int numProcessors=64;

/* Maximum number of kernel threads */
static const unsigned MAX_THREADS = 1024;

/* Maximum number of VG translations */
#ifdef VG
  #define MAX_TRANSLATIONS 4096
#else
  #define MAX_TRANSLATIONS 0
#endif // VG

#endif
