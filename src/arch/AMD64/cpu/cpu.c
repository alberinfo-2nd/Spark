#include <arch/AMD64/cpu/cpu.h>

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

inline void X86_CPU_set_cr4_bit(u8 bit) {
    u64 current_cr4 = 0;
    asm volatile("mov %%cr4, %0" : "=a" (current_cr4) : : );
    current_cr4 |= 1 << bit;
    asm volatile("mov %0, %%cr4" : : "a" (current_cr4) : );
}