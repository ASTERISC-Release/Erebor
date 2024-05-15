//===-- stack.h -----------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Stack-switching and other secure entry point operations.
//
//===----------------------------------------------------------------------===//
//
// Defines macro that can be used to define functions that act as
// entry points into the secure kernel.
//
// Sample use:
//
// SECURE_WRAPPER(int, TestFunc, int a, int b) {
//  return a + b;
//}
//
// This will define a function 'TestFunc' that acts as a wrapper
// for 'TestFunc_secure' which will contain the usual function code.
//
// The wrapper function handles operations needed to enter secure
// execution (see SECURE_ENTRY macro for high-level description)
// and return to normal execution (see SECURE_EXIT).
//
// As implemented, these are not reentrant.
//
//===----------------------------------------------------------------------===//
//
// TODO:
//  * Use per-CPU stacks for SMP operation
//
//===----------------------------------------------------------------------===//

#ifndef _STACK_SWITCH_
#define _STACK_SWITCH_

// #include <stdint.h>

//===-- Secure Stack Switching --------------------------------------------===//

// Points to top of secure stack
// XXX: Whoever defines this should ensure the region is write-protected!
extern const uintptr_t SecureStackBase;

// TODO: Manage stack per-cpu, do lookup here
// Use only RAX/RCX registers to accomplish this.
// (Or spill more in calling context)

#if defined(CONFIG_ENCOS) && defined(CONFIG_ENCOS_STACK)

#define SWITCH_TO_SECURE_STACK                                                 \
  /* Spill registers for temporary use */                                      \
  "movq %rax, -8(%rsp)\n"                                                      \
  "movq %rcx, -16(%rsp)\n"                                                     \
  /* Get the processor ID */                                                   \
  "rdtscp\n"                                                                   \
  /* Find the secure stack offset for the processor ID */                      \
  "andq $0xFFF, %rcx\n"                                                        \
  "movq %rcx, %rax\n"                                                          \
  "movq $4096, %rcx\n"                                                         \
  "mulq %rcx\n"                                                                \
  "addq SecureStackBase, %rax\n"                                               \
  /* Save normal stack pointer in rcx */                                       \
  "movq %rsp, %rcx\n"                                                          \
  /* Switch to secure stack! */                                                \
  "movq %rax, %rsp\n"                                                          \
  /* Save original stack pointer on Secure Stack for later restoration */      \
  "pushq %rcx\n"                                                               \
  /* Restore spilled registers from original stack (rcx) */                    \
  "movq -8(%rcx), %rax\n"                                                      \
  "movq -16(%rcx), %rcx\n"                                                     
  
#define SWITCH_BACK_TO_NORMAL_STACK                                            \
/* Switch back to original stack */                                            \
  "movq 0(%rsp), %rsp\n"                                                       

#else

#define SWITCH_TO_SECURE_STACK                                                 \

#define SWITCH_BACK_TO_NORMAL_STACK                                            \

#endif  /* CONFIG_ENCOS && CONFIG_ENCOS_STACK */

//===-- Interrupt Flag Control --------------------------------------------===//

#define DISABLE_INTERRUPTS                                                     \
  /* Save current flags */                                                     \
  "pushf\n"                                                                    \
  /* Disable interrupts */                                                     \
  "cli\n"

#define ENABLE_INTERRUPTS                                                      \
  /* Restore flags, enabling interrupts if they were before */                 \
  "popf\n"


//===-- PKS-Protect Control ---------------------------------------------===//

#if defined(CONFIG_ENCOS) && defined(CONFIG_ENCOS_PKS)

#define ENABLE_PKS_PROTECTION                                                  \
  /* Save scratch register to stack */                                         \
  "movq %rax, -8(%rsp)\n"                                                      \
  "movq %rdx, -16(%rsp)\n"                                                     \
  "movq %rcx, -24(%rsp)\n"                                                     \
  /* Write the PKRS MSR ID in rcx */                                           \
  "movq $0x6e1, %rcx\n"                                                        \
  /* Get current PKRS value */                                                 \
  "rdmsr\n"                                                                    \
  /* Restrict all access for key 1 */                                          \
  "orq $0x0000000000000008, %rax\n"                                            \
  /* Update the PKRS value */                                                  \
  "wrmsr\n"                                                                    \
  /* Restore clobbered register */                                             \
  "movq -8(%rsp), %rax\n"                                                      \
  "movq -16(%rsp), %rdx\n"                                                     \
  "movq -24(%rsp), %rcx\n"                                                     

#define DISABLE_PKS_PROTECTION                                                 \
  /* Save scratch register to stack */                                         \
  "movq %rax, -8(%rsp)\n"                                                      \
  "movq %rdx, -16(%rsp)\n"                                                     \
  "movq %rcx, -24(%rsp)\n"                                                     \
  /* Write the PKRS MSR ID in rcx */                                           \
  "movq $0x6e1, %rcx\n"                                                        \
  /* Get current PKRS value */                                                 \
  "rdmsr\n"                                                                    \
  /* Allow access for key 1 */                                                 \
  "andq $0xFFFFFFFFFFFFFFF7, %rax\n"                                           \
  /* Update the PKRS value */                                                  \
  "wrmsr\n"                                                                    \
  /* Restore clobbered register */                                             \
  "movq -8(%rsp), %rax\n"                                                      \
  "movq -16(%rsp), %rdx\n"                                                     \
  "movq -24(%rsp), %rcx\n"                                                     \
  "cli\n"                                                                      

#else 

#define ENABLE_PKS_PROTECTION                                                  \

#define DISABLE_PKS_PROTECTION                                                 \

#endif  /* CONFIG_ENCOS && CONFIG_ENCOS_PKS */


//===-- Entry/Exit High-Level Descriptions --------------------------------===//

#if (0)

#define SECURE_ENTRY                                                           \
  DISABLE_INTERRUPTS                                                           \
  DISABLE_PKS_PROTECTION                                                       \
  SWITCH_TO_SECURE_STACK

#define SECURE_EXIT                                                            \
  SWITCH_BACK_TO_NORMAL_STACK                                                  \
  ENABLE_PKS_PROTECTION                                                        \
  ENABLE_INTERRUPTS

#else
// More optimized variants

#define SECURE_ENTRY                                                           \
  /* Save current flags */                                                     \
  "pushf\n"                                                                    \
  /* Disable interrupts */                                                     \
  "cli\n"                                                                      \
  /* Spill registers for temporary use */                                      \
  "movq %rax, -8(%rsp)\n"                                                      \
  "movq %rdx, -16(%rsp)\n"                                                     \
  "movq %rcx, -24(%rsp)\n"                                                     \
  /* Write the PKRS MSR ID in rcx */                                           \
  "movq $0x6e1, %rcx\n"                                                        \
  /* Get current PKRS value */                                                 \
  "rdmsr\n"                                                                    \
  /* Allow access for key 1 */                                                 \
  "andq $0xFFFFFFFFFFFFFFF7, %rax\n"                                           \
  /* Update the PKRS value */                                                  \
  "wrmsr\n"                                                                    \
  /* Disable interrupts */                                                     \
  "cli\n"                                                                      \
  /* Get the processor ID */                                                   \
  "rdtscp\n"                                                                   \
  /* Find the secure stack offset for the processor ID */                      \
  "andq $0xFFF, %rcx\n"                                                        \
  "movq %rcx, %rax\n"                                                          \
  "movq $4096, %rcx\n"                                                         \
  "mulq %rcx\n"                                                                \
  "addq SecureStackBase, %rax\n"                                               \
  /* Save initial stack pointer in rcx */                                      \
  "movq %rsp, %rcx\n"                                                          \
  /* Switch to secure stack! */                                                \
  "movq %rax, %rsp\n"                                                          \
  /* Save original stack pointer for later restoration */                      \
  "pushq %rcx\n"                                                               \
  /* Restore spilled registers from original stack (rcx) */                    \
  "movq -8(%rcx), %rax\n"                                                      \
  "movq -16(%rcx), %rdx\n"                                                     \
  "movq -24(%rcx), %rcx\n"                                                     \

#define SECURE_EXIT                                                            \
  /* Switch back to original stack */                                          \
  "movq 0(%rsp), %rsp\n"                                                       \
  /* Save scratch register to stack */                                         \
  "pushq %rax\n"                                                               \
  "pushq %rcx\n"                                                               \
  "pushq %rdx\n"                                                               \
  /* Write the PKRS MSR ID in rcx */                                           \
  "movq $0x6e1, %rcx\n"                                                        \
  /* Get current PKRS value */                                                 \
  "rdmsr\n"                                                                    \
  /* Restrict all access for key 1 */                                          \
  "orq $0x0000000000000008, %rax\n"                                            \
  /* Update the PKRS value */                                                  \
  "wrmsr\n"                                                                    \
  /* Restore clobbered register */                                             \
  "popq %rdx\n"                                                                \
  "popq %rcx\n"                                                                \
  "popq %rax\n"                                                                \
  /* Restore flags, enabling interrupts if they were before */                 \
  "popf\n"

#endif

#define SECURE_INTERRUPT_REDIRECT                                              \
  DISABLE_INTERRUPTS                                                           \
  ENABLE_PKS_PROTECTION                                                        \
  ENABLE_INTERRUPTS                                                            \


//===-- Wrapper macro for marking Secure Entrypoints ----------------------===//

#define SECURE_WRAPPER(RET, FUNC, ...) \
asm( \
  ".text\n" \
  ".globl " #FUNC "\n" \
  ".align 16,0x90\n" \
  ".type " #FUNC ",@function\n" \
  #FUNC ":\n" \
  /* Do whatever's needed on entry to secure area */ \
  SECURE_ENTRY \
  /* Call real version of function */ \
  "call " #FUNC "_secure\n" \
  /* Operation complete, go back to unsecure mode */ \
  SECURE_EXIT \
  "ret\n" \
  #FUNC "_end:\n" \
  ".size " #FUNC ", " #FUNC "_end - " #FUNC "\n" \
); \
RET FUNC ##_secure(__VA_ARGS__); \
RET FUNC ##_secure(__VA_ARGS__)

#define SECURE_WRAPPER_INTERRUPT(RET, FUNC, ...) \
asm( \
  ".text\n" \
  ".globl " #FUNC "\n" \
  ".align 16,0x90\n" \
  ".type " #FUNC ",@function\n" \
  #FUNC ":\n" \
  SECURE_INTERRUPT_REDIRECT \
  /* Call real version of function */ \
  "call " #FUNC "_intr\n" \
  "ret\n" \
  /* Operation complete, go back to unsecure mode */ \
  #FUNC "_end:\n" \
  ".size " #FUNC ", " #FUNC "_end - " #FUNC "\n" \
); \
RET FUNC ##_intr(__VA_ARGS__); \
RET FUNC ##_intr(__VA_ARGS__)

//===-- Wrapper macro for calling secure functions from secure context ---===//

#define SECURE_CALL(FUNC, ...) \
{ \
    extern typeof(FUNC) FUNC ##_secure; \
    (void)FUNC ##_secure(__VA_ARGS__); \
}

//===-- Utilities for accessing original context from secure functions ----===//

static inline uintptr_t get_insecure_context_rsp(void) {
  // Original RSP is first thing put on the secure stack:
  uintptr_t *ptr = (uintptr_t *)SecureStackBase;
  return ptr[-1];
}

static inline uintptr_t get_insecure_context_flags(void) {
  // Insecure flags are stored on the insecure stack:
  uintptr_t *ptr = (uintptr_t *)get_insecure_context_rsp();
  return ptr[0];
}

static inline uintptr_t get_insecure_context_return_addr(void) {
  // Original insecure return address should be above the flags:
  // XXX: This is untested!
  uintptr_t *ptr = (uintptr_t *)get_insecure_context_rsp();
  return ptr[1];
}

#endif // _STACK_SWITCH_
