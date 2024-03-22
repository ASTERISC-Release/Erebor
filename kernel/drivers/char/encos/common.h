#ifndef __ENCOS_COMMON_H__
#define __ENCOS_COMMON_H__

#define pr_fmt(fmt) "encdev-ctl: " fmt

#include <linux/kernel.h>

#define log_info(fmt, arg...) \
    printk(KERN_INFO "[%s][%d] "pr_fmt(fmt)"", __func__, __LINE__, ##arg)
#define log_err(fmt, arg...) \
    printk(KERN_ERR "[%s][%d] "pr_fmt(fmt)"", __func__, __LINE__, ##arg)

#endif