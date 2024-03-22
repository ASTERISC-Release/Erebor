#include "common.h"

#include <linux/mm.h>
#include <linux/cma.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/sched.h>
#include <linux/debugfs.h>
#include <linux/io.h>
#include <linux/moduleparam.h>
#include <linux/delay.h>
#include <linux/highmem.h>
#include <asm/pgtable.h>
#include <linux/kallsyms.h>
#include <linux/fs.h>

#include "encos_alloc.h"


#define DEV_NAME "encos-dev"

struct miscdevice *misc;
struct mutex encos_dev_mlock;


static struct file_operations encos_dev_ops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = NULL,
    .mmap = NULL,
};

static int __init encos_dev_init(void)
{
    int rvl;
    encos_mem_t *mem; // debug

    /* register device */
    misc = kzalloc(sizeof(struct miscdevice), GFP_KERNEL);
    misc->name = DEV_NAME;
    misc->minor = MISC_DYNAMIC_MINOR;
    misc->fops = &encos_dev_ops;

    rvl = misc_register(misc);
    if (rvl) {
        log_err("Failed to register misc device: %s\n", DEV_NAME);
        return rvl;
    }

    /* lock init */
    mutex_init(&encos_dev_mlock);
    /* allocator init */
    init_encos_allocator();
    
    /* debug: allocate a chunk of memory */
    mem = encos_alloc(/*size=*/0x1000, /*enc_id=*/0);
    encos_mem_inspect(mem);

    log_info("Initialized.\n");
    return 0;
}

static void __exit encos_dev_exit(void)
{
    /* deregister device */
    misc_deregister(misc);
    kfree(misc);

    /* destroy allocator */
    destory_encos_allocator();

    log_info("Exit.\n");
}

module_init(encos_dev_init);
module_exit(encos_dev_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Chuqi Zhang");
MODULE_DESCRIPTION("ENCOS device driver");