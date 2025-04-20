#ifndef CPU_H
#define CPU_H

#include <types.h>

inline void X86_CPU_cli(void) {
    asm volatile("cli");
}

inline void X86_CPU_sti(void) {
    asm volatile("sti");
}

inline void X86_CPU_hlt(void) {
    asm volatile("hlt");
}

inline void X86_CPU_cpuid(u32 function, u32 *eax, u32 *ebx, u32 *ecx, u32 *edx) {
    asm volatile("cpuid" : "=a" (*eax), "=b" (*ebx), "=c" (*ecx), "=d" (*edx) : "a" (function));
}

extern void X86_CPU_set_cr4_bit(u8 bit);

inline void X86_CPU_set_cr3(void* addr) {
    asm volatile("mov %0, %%cr3" : : "a" (addr) : "memory");
}

#endif