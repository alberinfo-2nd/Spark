#include "kernel/debug/log.h"
#include <kernel/panic/panic.h>
#include <arch/AMD64/cpu/cpu.h>

void kpanic(string message, struct ISF_t *regs) {
    DEBUG_log((const string)"\nPANIC!!! Spark is unable to continue safely.");
    DEBUG_log((const string)"Spark was halted with the following message: %s", message);
    if (regs == NULL) {
        DEBUG_log((const string)"No cpu state was provided.");
    } else {
        DEBUG_log((const string)"--------- CPU STATE ---------");
        DEBUG_log((const string)"RAX: %x, RBX: %x, RCX: %x", regs->rax, regs->rbx, regs->rcx);
        DEBUG_log((const string)"RDX: %x, RDI: %x, RSI: %x", regs->rdx, regs->rdi, regs->rsi);
        DEBUG_log((const string)"R8: %x, R9: %x, R10: %x", regs->r8, regs->r9, regs->r10);
        DEBUG_log((const string)"R11: %x, R12: %x , R13: %x", regs->r11, regs->r12, regs->r13);
        DEBUG_log((const string)"R14: %x, R15: %x", regs->r14, regs->r15);
        DEBUG_log((const string)"RSP: %x", regs->rsp);
        DEBUG_log((const string)"RIP: %x", regs->rip);
        DEBUG_log((const string)"CS: %x, SS: %x", regs->cs, regs->ss);
        DEBUG_log((const string)"FLAGS: %x", regs->rflags);
        DEBUG_log((const string)"----------------------------");
    }
    DEBUG_log((const string)"Computer Halted.");
    
    while (true) {
        X86_CPU_cli();
        X86_CPU_hlt();
    }
}