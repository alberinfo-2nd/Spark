#include <boot/entry.h>
#include <arch/AMD64/cpu/gdt.h>
#include <arch/AMD64/cpu/idt.h>

void kentry(void* multiboot_data, void* PML4) {
    install_gdt(true, 0);
    install_idt(true, 0);

    for(;;);
}