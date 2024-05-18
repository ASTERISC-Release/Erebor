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

static inline long syscall_3(long syscall_number, long arg1, long arg2, long arg3) {
    long result;

    asm volatile (
        "syscall"
        : "=a" (result)
        : "a" (syscall_number), "D" (arg1), "S" (arg2), "d" (arg3)
        : "rcx", "r11", "memory"
    );

    return result;
}