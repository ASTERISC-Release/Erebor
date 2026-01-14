#include <sva/stats.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/pid.h>
#include <asm/current.h>

// spinlock_t stats_lock;
stats_t stats;

void stats_init(int process_group_id) {
    // spin_lock_init(&stats_lock);
    // spin_lock(&stats_lock);
    memset(&stats, 0, sizeof(stats_t));
    stats.process_group_id = process_group_id;
    // spin_unlock(&stats_lock);
}

void stats_syscall_incr(int syscall_no) {
    if(strcmp(current->comm, "benchmark") == 0 || stats.process_group_id == 0 || (task_pgrp(current) && task_pgrp(current)->numbers[0].nr != stats.process_group_id))
        return;

    printk("task_name = %s, PID: %d, PGID: %d\n", current->comm, current->pid, task_pgrp(current)->numbers[0].nr);

    int cpu = smp_processor_id();
    // int cpu = 0;
    // spin_lock(&stats_lock);    
    stats.syscall[syscall_no][cpu]++;
    // spin_unlock(&stats_lock);    

}

void stats_svacall_incr(int svacall_no) {
    if(strcmp(current->comm, "benchmark") == 0 || stats.process_group_id == 0 || (task_pgrp(current) && task_pgrp(current)->numbers[0].nr != stats.process_group_id))
        return;

    printk("task_name = %s, PID: %d, PGID: %d\n", current->comm, current->pid, task_pgrp(current)->numbers[0].nr);
    int cpu = smp_processor_id();
    // int cpu = 0;
    // spin_lock(&stats_lock);    
    stats.svacall[svacall_no][cpu]++;
    // spin_unlock(&stats_lock);    
}

void stats_interrupt_incr(int interrupt_no) {
    if(strcmp(current->comm, "benchmark") == 0 || stats.process_group_id == 0 || (task_pgrp(current) && task_pgrp(current)->numbers[0].nr != stats.process_group_id))
        return;

    printk("task_name = %s, PID: %d, PGID: %d\n", current->comm, current->pid, task_pgrp(current)->numbers[0].nr);

    int cpu = smp_processor_id();
    // int cpu = 0;
    // spin_lock(&stats_lock);    
    stats.interrupt[interrupt_no][cpu]++;
    // spin_unlock(&stats_lock);    
}