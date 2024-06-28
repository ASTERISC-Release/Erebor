#ifndef _SVA_STATS
#define _SVA_STATS

#include <asm/unistd_64.h>
#include <linux/spinlock.h>

/* 
    - Creating just one stats object for simplicity
    - The object is representative of the stats for a process, and is associated with 
    the process using the name of the binary
    - Does not support collecting collection of stats of multiple processes in parallel
*/

#define NR_SYSCALL      454
#define NR_INTERRUPT    256
#define NR_SVACALL      38
#define NCPU            8

// extern spinlock_t stats_lock;

typedef struct stats {
    int process_group_id;
    int syscall[NR_SYSCALL];    
    int interrupt[NR_INTERRUPT];    
    int svacall[NR_SVACALL];
    int val;
} stats_t;

extern stats_t stats;

extern void stats_init(int process_group_id);
extern void stats_syscall_incr(int syscall_no);
extern void stats_svacall_incr(int svacall_no);
extern void stats_interrupt_incr(int interrupt_no);

#endif