#include "arch/AMD64/mmu/mmu.h"
#include <arch/AMD64/cpu/cpu.h>
#include <kernel/mm/kalloc.h>

#define MSR_FSBase 0xC0000100
#define MSR_GSBase 0xC0000101
#define MSR_kernelGSBase 0xC0000102

void X86_CPU_set_cr4_bit(u8 bit) {
    u64 current_cr4 = 0;
    asm volatile("mov %%cr4, %0" : "=a" (current_cr4) : : );
    current_cr4 |= 1 << bit;
    asm volatile("mov %0, %%cr4" : : "a" (current_cr4) : );
}

extern struct X86_CPU_self_t* X86_CPU_get_self(void) {
    void* ptr = 0;
    asm volatile("mov %%gs:0, %0" : : "*a" (ptr));
}

extern void X86_CPU_create_self() {
    struct X86_CPU_self_t* cpu_self = (struct X86_CPU_self_t*)MMU_make_addr_half(kalloc(sizeof(struct X86_CPU_self_t)), MMU_addr_higher_half);
    cpu_self->cpuID = X86_CPU_get_cpuid();
    //set GDT and IDT

    X86_CPU_wrmsr(MSR_kernelGSBase, (u64)cpu_self);
    asm volatile("swapgs"); //swap kernelGSBase into GS.Base
}