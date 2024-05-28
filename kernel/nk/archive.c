/*
 * Function: sva_wrmsr
 *
 * Description:
 *  SVA Intrinsic to load a value in an MSR. The given value should be
 *  given in edx:eax and the MSR should be given in ecx. If the MSR is
 *  EFER, we need to make sure that the NXE bit is enabled. 
 */
// void
// sva_wrmsr() {
//     uint64_t val;
//     unsigned int msr;
//     __asm__ __volatile__ (
//         "wrmsr\n"
//         : "=c" (msr), "=a" (val)
//         :
//         : "rax", "rcx", "rdx"
//     );
//     if ((msr == MSR_REG_EFER) && !(val & EFER_NXE)) {
//       // panic("SVA: attempt to clear the EFER.NXE bit: %x.", val);
//     }
// }