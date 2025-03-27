#include <boot/entry.h>
#include <arch/AMD64/cpu/gdt.h>
#include <arch/AMD64/interrupt/pic.h>
#include <arch/AMD64/cpu/idt.h>

void kentry(void* multiboot_data, void* PML4) {
    install_gdt(true, 0);
    X86_PIC_remap(32, 32+8); //Set the master pic to start in the 32nd entry of the IDT (32nd interrupt vector), and the slave PIC on the 40th
    install_idt(true, 0);

    asm volatile("sti");

    for(;;);
}