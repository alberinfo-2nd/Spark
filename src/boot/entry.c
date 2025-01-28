#include <boot/entry.h>
#include <arch/AMD64/cpu/gdt.h>

void kentry(void* multiboot_data, void* PML4) {
    install_gdt(true, 0);

    for(;;);
}