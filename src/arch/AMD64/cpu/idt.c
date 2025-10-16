#include "arch/AMD64/cpu/cpu.h"
#include "arch/AMD64/mmu/mmu.h"
#include "kernel/mm/kalloc.h"
#include <arch/AMD64/cpu/idt.h>

#define DPL_KERNEL  0
#define DPL_USER    0b11

#define DESCRIPTOR_LDT              0b0010
#define DESCRIPTOR_AVAILABLE_TSS    0b1001
#define DESCRIPTOR_BUSY_TSS         0b1011
#define DESCRIPTOR_CALL_GATE        0b1100
#define DESCRIPTOR_INTERRUPT        0b1110
#define DESCRIPTOR_TRAP_GATE        0b1111

extern void ISR_0(void);
extern void ISR_1(void);
extern void ISR_2(void);
extern void ISR_3(void);
extern void ISR_4(void);
extern void ISR_5(void);
extern void ISR_6(void);
extern void ISR_7(void);
extern void ISR_8(void);
extern void ISR_9(void);
extern void ISR_10(void);
extern void ISR_11(void);
extern void ISR_12(void);
extern void ISR_13(void);
extern void ISR_14(void);
extern void ISR_15(void);
extern void ISR_16(void);
extern void ISR_17(void);
extern void ISR_18(void);
extern void ISR_19(void);
extern void ISR_20(void);
extern void ISR_21(void);
extern void ISR_22(void);
extern void ISR_23(void);
extern void ISR_24(void);
extern void ISR_25(void);
extern void ISR_26(void);
extern void ISR_27(void);
extern void ISR_28(void);
extern void ISR_29(void);
extern void ISR_30(void);
extern void ISR_31(void);

extern void IRQ_0(void);
extern void IRQ_1(void);
extern void IRQ_2(void);
extern void IRQ_3(void);
extern void IRQ_4(void);
extern void IRQ_5(void);
extern void IRQ_6(void);
extern void IRQ_7(void);
extern void IRQ_8(void);
extern void IRQ_9(void);
extern void IRQ_10(void);
extern void IRQ_11(void);
extern void IRQ_12(void);
extern void IRQ_13(void);
extern void IRQ_14(void);
extern void IRQ_15(void);

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
    u32 offset_high;
    u32 reserved_2;
} __attribute__((packed));

struct IDT_t {
    struct IDT_Gate_t entries[256];
    struct IDTR_t ptr;
} __attribute__((packed)) __attribute__((aligned(0x20))); //Align to Doubleword (32-bits)

void X86_IDT_setup_entry(struct IDT_Gate_t* entry, void* call_addr, u8 IST, u8 gate_type, u8 DPL);
void X86_IDT_setup(struct IDT_t* IDT);
void isr_handler(struct ISF_t* regs);

void X86_IDT_setup_entry(struct IDT_Gate_t* entry, void* call_addr, u8 IST, u8 gate_type, u8 DPL) {
    entry->offset_low = (u16)((u64)call_addr & 0xFFFF); //Low 16 bits
    entry->offset_mid = (u16)((u64)call_addr >> 16 & 0xFFFF); //Middle 16 bits
    entry->offset_high = (u32)((u64)call_addr >> 32); //High 32 bits

    if(DPL > 3) DPL = 3; //If DPL is wrong, then assume its user-mode
    entry->DPL = DPL;

    if(DPL == 3) entry->segment = 0x18; //User-mode CS
    else entry->segment = 0x8; //Kernel-mode CS

    entry->gate_type = gate_type;

    if(IST > 8) IST = 0; //Discard invalid values
    entry->IST = IST;

    entry->present = 1;

    entry->zero = 0;
    entry->reserved_1 = 0;
    entry->reserved_2 = 0;
}

void X86_IDT_setup(struct IDT_t* IDT) {
    // Setup IDT Entries for ISRs and IRQs
    X86_IDT_setup_entry(&IDT->entries[0], &ISR_0, 0, DESCRIPTOR_TRAP_GATE, DPL_KERNEL);
    X86_IDT_setup_entry(&IDT->entries[1], &ISR_1, 0, DESCRIPTOR_TRAP_GATE, DPL_KERNEL);
    X86_IDT_setup_entry(&IDT->entries[2], &ISR_2, 0, DESCRIPTOR_TRAP_GATE, DPL_KERNEL);
    X86_IDT_setup_entry(&IDT->entries[3], &ISR_3, 0, DESCRIPTOR_TRAP_GATE, DPL_KERNEL);
    X86_IDT_setup_entry(&IDT->entries[4], &ISR_4, 0, DESCRIPTOR_TRAP_GATE, DPL_KERNEL);
    X86_IDT_setup_entry(&IDT->entries[5], &ISR_5, 0, DESCRIPTOR_TRAP_GATE, DPL_KERNEL);
    X86_IDT_setup_entry(&IDT->entries[6], &ISR_6, 0, DESCRIPTOR_TRAP_GATE, DPL_KERNEL);
    X86_IDT_setup_entry(&IDT->entries[7], &ISR_7, 0, DESCRIPTOR_TRAP_GATE, DPL_KERNEL);
    X86_IDT_setup_entry(&IDT->entries[8], &ISR_8, 0, DESCRIPTOR_TRAP_GATE, DPL_KERNEL);
    X86_IDT_setup_entry(&IDT->entries[10], &ISR_10, 0, DESCRIPTOR_TRAP_GATE, DPL_KERNEL);
    X86_IDT_setup_entry(&IDT->entries[11], &ISR_11, 0, DESCRIPTOR_TRAP_GATE, DPL_KERNEL);
    X86_IDT_setup_entry(&IDT->entries[12], &ISR_12, 0, DESCRIPTOR_TRAP_GATE, DPL_KERNEL);
    X86_IDT_setup_entry(&IDT->entries[13], &ISR_13, 0, DESCRIPTOR_TRAP_GATE, DPL_KERNEL);
    X86_IDT_setup_entry(&IDT->entries[14], &ISR_14, 0, DESCRIPTOR_TRAP_GATE, DPL_KERNEL);
    X86_IDT_setup_entry(&IDT->entries[16], &ISR_16, 0, DESCRIPTOR_TRAP_GATE, DPL_KERNEL);
    X86_IDT_setup_entry(&IDT->entries[17], &ISR_17, 0, DESCRIPTOR_TRAP_GATE, DPL_KERNEL);
    X86_IDT_setup_entry(&IDT->entries[18], &ISR_18, 0, DESCRIPTOR_TRAP_GATE, DPL_KERNEL);
    X86_IDT_setup_entry(&IDT->entries[19], &ISR_19, 0, DESCRIPTOR_TRAP_GATE, DPL_KERNEL);
    X86_IDT_setup_entry(&IDT->entries[20], &ISR_20, 0, DESCRIPTOR_TRAP_GATE, DPL_KERNEL);
    X86_IDT_setup_entry(&IDT->entries[21], &ISR_21, 0, DESCRIPTOR_TRAP_GATE, DPL_KERNEL);
    X86_IDT_setup_entry(&IDT->entries[28], &ISR_28, 0, DESCRIPTOR_TRAP_GATE, DPL_KERNEL);
    X86_IDT_setup_entry(&IDT->entries[29], &ISR_29, 0, DESCRIPTOR_TRAP_GATE, DPL_KERNEL);
    X86_IDT_setup_entry(&IDT->entries[30], &ISR_30, 0, DESCRIPTOR_TRAP_GATE, DPL_KERNEL);

    X86_IDT_setup_entry(&IDT->entries[32], &IRQ_0, 0, DESCRIPTOR_INTERRUPT, DPL_KERNEL);
    X86_IDT_setup_entry(&IDT->entries[33], &IRQ_1, 0, DESCRIPTOR_INTERRUPT, DPL_KERNEL);
    X86_IDT_setup_entry(&IDT->entries[34], &IRQ_2, 0, DESCRIPTOR_INTERRUPT, DPL_KERNEL);
    X86_IDT_setup_entry(&IDT->entries[35], &IRQ_3, 0, DESCRIPTOR_INTERRUPT, DPL_KERNEL);
    X86_IDT_setup_entry(&IDT->entries[36], &IRQ_4, 0, DESCRIPTOR_INTERRUPT, DPL_KERNEL);
    X86_IDT_setup_entry(&IDT->entries[37], &IRQ_5, 0, DESCRIPTOR_INTERRUPT, DPL_KERNEL);
    X86_IDT_setup_entry(&IDT->entries[38], &IRQ_6, 0, DESCRIPTOR_INTERRUPT, DPL_KERNEL);
    X86_IDT_setup_entry(&IDT->entries[39], &IRQ_7, 0, DESCRIPTOR_INTERRUPT, DPL_KERNEL);
    X86_IDT_setup_entry(&IDT->entries[40], &IRQ_8, 0, DESCRIPTOR_INTERRUPT, DPL_KERNEL);
    X86_IDT_setup_entry(&IDT->entries[41], &IRQ_9, 0, DESCRIPTOR_INTERRUPT, DPL_KERNEL);
    X86_IDT_setup_entry(&IDT->entries[42], &IRQ_10, 0, DESCRIPTOR_INTERRUPT, DPL_KERNEL);
    X86_IDT_setup_entry(&IDT->entries[43], &IRQ_11, 0, DESCRIPTOR_INTERRUPT, DPL_KERNEL);
    X86_IDT_setup_entry(&IDT->entries[44], &IRQ_12, 0, DESCRIPTOR_INTERRUPT, DPL_KERNEL);
    X86_IDT_setup_entry(&IDT->entries[45], &IRQ_13, 0, DESCRIPTOR_INTERRUPT, DPL_KERNEL);
    X86_IDT_setup_entry(&IDT->entries[46], &IRQ_14, 0, DESCRIPTOR_INTERRUPT, DPL_KERNEL);
    X86_IDT_setup_entry(&IDT->entries[47], &IRQ_15, 0, DESCRIPTOR_INTERRUPT, DPL_KERNEL);

    IDT->ptr.offset = (u64)&IDT->entries;
    IDT->ptr.limit = sizeof(IDT->entries)-1;

    return;
}

void X86_IDT_install(bool is_bootcore, u32 cpuId) {
    struct IDT_t* IDT = MMU_make_addr_half(kalloc(sizeof(struct IDT_t)), MMU_addr_kernel_half);
    X86_IDT_setup(IDT);
    X86_CPU_get_self()->idt = IDT;

    asm volatile("lidt (%0)" : : "r" ((u64)&IDT->ptr));

    return;
}