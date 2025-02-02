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

#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/kconfig.h>
/* Determine whether the virtual ghost features are enabled */
#ifdef VG
static const unsigned char vg = 1;
#else
static const unsigned char vg = 0;
#endif

/* Total number of processors supported by this SVA Execution Engine */
#define NCPU              8

/* Maximum number of kernel threads */
static const unsigned MAX_THREADS = 1024;

/* Maximum number of VG translations */
#ifdef VG
  #define MAX_TRANSLATIONS 4096
#else
  #define MAX_TRANSLATIONS 0
#endif // VG

#undef pr_fmt
#define pr_fmt(fmt) "encos.SM: " fmt


/* Chuqi:
 * In principle, we cannot use a kernel printk function which is unsafe.
 * Anyways, we use those log functions only for debugging purpose. 
 */
#define log_info(fmt, arg...) \
    printk(KERN_INFO "[%s][%d] "pr_fmt(fmt)"", __func__, __LINE__, ##arg)
#define log_err(fmt, arg...) \
    printk(KERN_ERR "[%s][%d] "pr_fmt(fmt)"", __func__, __LINE__, ##arg)

/* Logging parameters and definitions local to the MMU configuration change */
enum log_type_t {
    PRINTK = 0,
    DECLARE,  /* Print all declares and printk */
    UPDATE,   /* Print all declares, printk, and updates */
};
#define VERBOSE PRINTK
#define DEBUG_WALK 1
// #define DEBUG_WALK 0

#define LOG_WALK(verb, args...) { \
  if (DEBUG_WALK >= verb) { \
    printk("SVA: (walk) " args); \
  } else {} }

#define LOG_GENERAL(verb, args...) { \
  if (DEBUG_WALK >= verb) { \
    printk("SVA: (walk) " args); \
  } else {} }

#define LOG_PRINTK(args...) printk("SVA: " args)
#define LOG_VIOLATION(args...) printk("[*]Violation: " args)

#define LOG_DECLARE(args...) { \
  if (VERBOSE>=DECLARE) { \
    printk("SVA: (declare) " args); \
  } else {} }

#define LOG_UPDATE(args...) { \
  if (VERBOSE>=UPDATE) { \
    printk("SVA: (update) " args); \
  } else {} }

#endif
