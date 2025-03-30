#include <arch/AMD64/cpu/cpu.h>

void X86_CPU_set_cr4_bit(u8 bit) {
    u64 current_cr4 = 0;
    asm volatile("mov %%cr4, %0" : "=a" (current_cr4) : : );
    current_cr4 |= 1 << bit;
    asm volatile("mov %0, %%cr4" : : "a" (current_cr4) : );
}