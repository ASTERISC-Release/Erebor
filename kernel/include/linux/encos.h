#ifndef _LINUX_ENCOS_H_
#define _LINUX_ENCOS_H_

#undef pr_fmt
#define pr_fmt(fmt) "encos: " fmt

#define ENCOS_DEBUG   (1)

/* ENCOS driver IOCTL interfaces */
#define   ENCOS_DEV_NAME "encos-dev"

#define   ENCOS_ENCLAVE_REQUEST         _IOW('m', 1, unsigned int)
#define   ENCOS_ENABLE_KDBG             _IOW('m', 2, unsigned int)
#define   ENCOS_DISABLE_KDBG            _IOW('m', 3, unsigned int)
#define         HHKR_MMAP_BUF           _IOW('m', 4, unsigned int)
#define         HHKR_MMAP_MSG_QUEUE     _IOW('m', 5, unsigned int)
#define         HHKR_MMAP_FREEBACK_WL   _IOW('m', 7, unsigned int)


#define log_info(fmt, arg...) \
    printk(KERN_INFO "[%s][%d] "pr_fmt(fmt)"", __func__, __LINE__, ##arg)
#define log_err(fmt, arg...) \
    printk(KERN_ERR "[%s][%d] "pr_fmt(fmt)"", __func__, __LINE__, ##arg)


#define ENCOS_ASSERT(cond, fmt, arg...) \
    do { \
        if (!(cond)) { \
            log_err(fmt, ##arg); \
            /* Optional: Add any error handling here. */ \
        } \
    } while (0)

extern int encos_kdbg_enabled;

#define log_kdbg(fmt, arg...) \
    do { \
        if (encos_kdbg_enabled) { \
            printk(KERN_CRIT "[%s][%d] "pr_fmt(fmt)"", __func__, __LINE__, ##arg); \
        } \
    } while (0)

extern void encos_enclave_free_all(int enc_id, int owner_pid);

#endif