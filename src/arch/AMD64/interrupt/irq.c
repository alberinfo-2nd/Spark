#include <arch/AMD64/cpu/idt.h>
#include <arch/AMD64/interrupt/irq.h>
#include <arch/AMD64/interrupt/pic.h>

void irq_handler(struct ISF_t* regs) {
    X86_PIC_send_eoi(regs->interrupt_number);
    return;
}