#ifndef CPU_H
#define CPU_H

#include <types.h>

extern void X86_CPU_cli(void);
extern void X86_CPU_sti(void);
extern void X86_CPU_hlt(void);
extern void X86_CPU_cpuid(u32 function, u32 *eax, u32 *ebx, u32 *ecx, u32 *edx);
extern void X86_CPU_set_cr4_bit(u8 bit);

#endif