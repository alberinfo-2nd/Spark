#include <arch/AMD64/cpu/cpu.h>
#include <arch/AMD64/cpu/idt.h>
#include <arch/AMD64/interrupt/irq.h>
#include <arch/AMD64/interrupt/pic.h>
#include <kernel/timer/timer.h>

void irq_handler(struct ISF_t* regs) {
    switch (regs->interrupt_number) {
        case 0: //PIT/HPET
            TIMER_irq_handler(regs->interrupt_number);
            break;
        case 255: //Spurious interrupt, just continue
            return;
        default:
            break;
    }

    X86_PIC_send_eoi(regs->interrupt_number);
    if(X86_CPU_get_self()->apic->send_eoi != NULL) X86_CPU_get_self()->apic->send_eoi(regs->interrupt_number);
    return;
}