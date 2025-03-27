#include <arch/AMD64/cpu/cpu.h>

void X86_CPU_cli(void) {
    asm volatile("cli");
}

void X86_CPU_sti(void) {
    asm volatile("sti");
}