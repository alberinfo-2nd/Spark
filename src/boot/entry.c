#include <kernel/mm/vmm.h>
#include <boot/entry.h>
#include <boot/multiboot2.h>
#include <arch/AMD64/cpu/cpu.h>
#include <arch/AMD64/cpu/gdt.h>
#include <arch/AMD64/interrupt/pic.h>
#include <arch/AMD64/cpu/idt.h>
#include <arch/AMD64/mmu/mmu.h>
#include <kernel/panic/panic.h>
#include <kernel/debug/serial.h>
#include <kernel/timer/timer.h>
#include <kernel/debug/log.h>
#include <kernel/sync/spinlock.h>
#include <kernel/mm/pmm.h>

void kentry(void* multiboot_data) {
    DEBUG_SERIAL_init();

    MMU_init();

    PMM_init();
    scan_mboot(multiboot_data);

    X86_CPU_create_self();

    X86_GDT_install();
    X86_PIC_remap(32, 32+8); //Set the master pic to start in the 32nd entry of the IDT (32nd interrupt vector), and the slave PIC on the 40th
    X86_IDT_install(true, 0);

    VMM_init();

    TIMER_init();

    for(;;);
}