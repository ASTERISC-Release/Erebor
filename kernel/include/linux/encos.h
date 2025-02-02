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
#define   ENCOS_ENCLAVE_ACT             _IOW('m', 4, unsigned int)
#define   ENCOS_ENCLAVE_EXIT            _IOW('m', 5, unsigned int)
#define   HHKR_MMAP_FREEBACK_WL         _IOW('m', 7, unsigned int)
#define   ENCOS_STATS_INIT              _IOW('m', 8, unsigned int)
#define   ENCOS_STATS_READ              _IOW('m', 9, unsigned int)
#define   ENCOS_STAC_ENABLE             _IOW('m', 10, unsigned int)
#define   ENCOS_STAC_STATS              _IOW('m', 11, unsigned int)
#define   ENCOS_MEM_NUM_STATS           _IOW('m', 12, unsigned int)


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
extern int is_enclave_activate_ut(int pid);
extern void free_enclave_ut(int pid);

#endif