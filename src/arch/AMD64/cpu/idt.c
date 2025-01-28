#include <arch/AMD64/cpu/idt.h>

struct IDTR_t {
    u16 limit;
    u64 offset;
} __attribute__((packed));

struct IDT_Gate_t {
    u16 offset_low;
    u16 segment;
    u8 IST : 3;
    u8 reserved_1 : 5;
    u8 gate_type : 4;
    u8 zero : 1;
    u8 DPL : 2;
    u8 present : 1;
    u16 offset_mid;
    u64 offset_high;
    u64 reserved_2;
} __attribute__((packed));

struct IDT_t {
    struct IDT_Gate_t entries[256];
    struct IDTR_t ptr;
} __attribute__((packed)) __attribute__((aligned(0x20))); //Align to Doubleword (32-bits)

struct IDT_Table_t {
    struct IDT_t IDT;
    struct IDT_Table_t *next;
    u32 cpuId;
};

struct IDT_Table_t *setup_idt();

static struct IDT_Table_t IDT_Table = (struct IDT_Table_t){ 0 };

struct IDT_Table_t *setup_idt() {
    struct IDT_Table_t *Current_table = &IDT_Table;
    while(Current_table->next != 0) {
        Current_table = Current_table->next;
    }

    struct IDT_t *IDT = &Current_table->IDT;

    // Setup IDT Entries for ISRs and IRQs
}

void install_idt(bool is_bootcore, u32 cpuId) {
    if(!is_bootcore) {} //TODO: Allocate new IDT entry

    struct IDT_Table_t *Current_IDT_Table = setup_idt();

    Current_IDT_Table->cpuId = cpuId;

    asm volatile("lidt (%0)" : : "r" ((u64)&Current_IDT_Table->IDT.ptr));

    return;
}