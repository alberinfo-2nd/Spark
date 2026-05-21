#include <arch/AMD64/cpu/idt.h>
#include <arch/AMD64/interrupt/isr.h>
#include <kernel/panic/panic.h>
#include <kernel/mm/vmm.h>

#define ISR_PAGEFAULT 14

void isr_handler(struct ISF_t* regs) {
    //Which vector caused the fault?
    switch(regs->interrupt_number) {
        case ISR_PAGEFAULT:
            VMM_page_fault_handler(regs);
            break;
        default:
            kpanic("UNHANDLED ISR", regs);
            break;
    }
    return;
}