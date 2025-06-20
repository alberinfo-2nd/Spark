#include "kernel/debug/log.h"
#include <kernel/panic/panic.h>
#include <arch/AMD64/cpu/cpu.h>

void kpanic(string message, struct ISF_t *regs) {
    DEBUG_log((const string)"\nPANIC!!! Spark is unable to continue safely.\0");
    DEBUG_log((const string)"Spark was halted with the following message: %s\0", message);
    if (regs == NULL) {
        DEBUG_log((const string)"No cpu state was provided.\0");
    } else {
        DEBUG_log((const string)"Int# %x, Error code: %x\0", regs->interrupt_number, regs->error_code);
        u64 CR0 = 0, CR2 = 0, CR3 = 0, CR4 = 0, EFER = 0;
        asm volatile("mov %%cr0, %0\nmov %%cr2, %1\nmov %%cr3, %2\nmov %%cr4, %3" : "=r" (CR0), "=r" (CR2), "=r" (CR3), "=r" (CR4) : : );
        asm volatile("mov $0xC0000080, %%ecx\nrdmsr\nmov %%rax, %0" : "=r" (EFER) : : "ecx", "eax");
        DEBUG_log((const string)"--------- CPU STATE ---------");
        DEBUG_log((const string)"RAX: %x, RBX: %x, RCX: %x\0", regs->rax, regs->rbx, regs->rcx);
        DEBUG_log((const string)"RDX: %x, RDI: %x, RSI: %x\0", regs->rdx, regs->rdi, regs->rsi);
        DEBUG_log((const string)"R8: %x, R9: %x, R10: %x\0", regs->r8, regs->r9, regs->r10);
        DEBUG_log((const string)"R11: %x, R12: %x , R13: %x\0", regs->r11, regs->r12, regs->r13);
        DEBUG_log((const string)"R14: %x, R15: %x\0", regs->r14, regs->r15);
        DEBUG_log((const string)"RSP: %x\0", regs->rsp);
        DEBUG_log((const string)"RIP: %x\0", regs->rip);
        DEBUG_log((const string)"CS: %x, SS: %x\0", regs->cs, regs->ss);
        DEBUG_log((const string)"FLAGS: %x\0", regs->rflags);
        DEBUG_log((const string)"CR0: %x, CR2: %x, CR3: %x, CR4: %x\0", CR0, CR2, CR3, CR4);
        DEBUG_log((const string)"EFER: %x\0", EFER);
        DEBUG_log((const string)"----------------------------");
    }
    DEBUG_log((const string)"Computer Halted.\0");
    
    while (true) {
        X86_CPU_cli();
        X86_CPU_hlt();
    }
}