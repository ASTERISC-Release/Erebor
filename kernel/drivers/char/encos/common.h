#ifndef __ENCOS_COMMON_H__
#define __ENCOS_COMMON_H__

#include <linux/kernel.h>
#include <linux/encos.h>

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "encdev-ctl: " fmt

#endif