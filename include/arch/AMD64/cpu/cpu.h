#ifndef CPU_H
#define CPU_H

#include <types.h>
#include <arch/AMD64/cpu/gdt.h>
#include <arch/AMD64/cpu/idt.h>
#include <kernel/mm/vmm.h>

//Information about this cpu. Will be expanded in the future.
//Only used for the kernel, usermode will store its own information within a thread
struct X86_CPU_self_t {
    u32 cpuID; //Apic ID, preferably
    struct GDT_t* gdt;
    struct IDT_t* idt;
    struct TSS_t* tss;
    struct VMM_Address_Space_t* address_space;
    // + possibly more data, such as IOAPIC, LAPIC, a pointer to the numa domain, tsc / timer info, etc
};

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

inline void X86_CPU_set_cr3(void* addr) {
    asm volatile("mov %0, %%cr3" : : "a" (addr) : "memory");
}

inline u8 X86_CPU_get_cpuid(void) {
    u32 ebx = 0, unused = 0;
    X86_CPU_cpuid(1, &unused, &ebx, &unused, &unused);
    return (u8)(ebx >> 24);
}

inline u64 X86_CPU_rdmsr(u32 msr) {
    u64 val = 0;
    asm volatile("rdmsr" : "=a" (*(u32*)&val), "=d" (*(u32*)(&val + 1)) : "c" (msr)); //
    return val;
}

inline void X86_CPU_wrmsr(u32 msr, u64 value) {
    asm volatile("wrmsr" : : "a" (value & ~(u32)1), "d" (value >> 32), "c" (msr));
}

extern void X86_CPU_set_cr4_bit(u8 bit);
extern struct X86_CPU_self_t* X86_CPU_get_self(void);
extern void X86_CPU_create_self(); //Will create a struct X86_CPU_self_t and put it in GS

#endif