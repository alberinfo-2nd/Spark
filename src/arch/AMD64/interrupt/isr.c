#include <arch/AMD64/cpu/idt.h>
#include <arch/AMD64/interrupt/isr.h>
#include <kernel/panic/panic.h>

void isr_handler(struct ISF_t* regs) {
    kpanic("UNHANDLED ISR", regs);
    return;
}