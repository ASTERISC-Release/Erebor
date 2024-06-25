#ifndef _ENCOS_PERF_H
#define _ENCOS_PERF_H


#include <linux/types.h>

#define N_TIMES     100

static inline uint64_t rdtscp(void) {
    unsigned int lo, hi;
    asm volatile(
        "rdtscp"
        : "=a"(lo), "=d"(hi)
        :
        : "memory"
    );
    return ((uint64_t)hi << 32) | lo;
}


static inline long hypercall_3(unsigned int nr, unsigned long p1,
				  unsigned long p2, unsigned long p3)
{
	long ret;
	asm volatile(
             "vmcall"
		     : "=a"(ret)
		     : "a"(nr), "b"(p1), "c"(p2), "d"(p3)
		     : "memory");
	return ret;
}

extern void encos_micro_perf(void);

#endif