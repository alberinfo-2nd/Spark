#include <arch/AMD64/cpu/apic.h>
#include <arch/AMD64/cpu/cpu.h>
#include <arch/AMD64/mmu/mmu.h>
#include <kernel/mm/vmm.h>
#include <kernel/mm/kalloc.h>

#define CPUID_APIC_SUPPORTED    1 << 9
#define CPUID_x2APIC_SUPPORTED  1 << 21

#define MSR_APIC_BASE_ADDR_REGISTER 0x0000001B
#define MSR_APIC_ENABLE             1 << 11
#define MSR_X2APIC_ENABLE           1 << 10
#define MSR_APIC_BSC                1 << 8 //Bootstrap CPU core, Read only
#define MSR_APIC_BASE_ADDR(x)       (x & ~((1 << 12) - 1)) //Base address has to be 4K aligned, and bits 0:11 are either reserved or separate fields noted above.

#define APIC_ID                 0x20
#define APIC_VERSION            0x30
    #define APIC_VERSION_x2APIC_PRESENT 1 << 31
    #define APIC_VERSION_MLE(x)         ((x >> 16) & ((1 << 8) - 1))
    #define APIC_VERSION_IMPL(x)        (x & ((1 << 8) - 1))
#define APIC_TPR                0x80
#define APIC_APR                0x90
#define APIC_PPR                0xA0
#define APIC_EOI                0xB0
#define APIC_RRR                0xC0
#define APIC_LDR                0xD0
#define APIC_DFR                0xE0
#define APIC_SPURIOUS           0xF0
    #define APIC_ASE            1 << 8
//#define APIC_ISR [0x100-0x170]; In-service register
//#define APIC_TMR [0x180-0x1F0]; Trigger mode register
//#define APIC_IRR [0x200-0x270]; Interrupt request register
#define APIC_ESR                0x280
#define APIC_INT_CMDReg_low     0x300
#define APIC_INT_CMDReg_high    0x310
#define APIC_TIMER_VECTOR       0x320
#define APIC_THERMAL_VECTOR     0x330
#define APIC_PERF_VECTOR        0x340
#define APIC_LINT0_VECTOR       0x350
#define APIC_LINT1_VECTOR       0x360
    #define APIC_VECTOR(x)      (x & 0xFF)
    #define APIC_VECTOR_MT(x)   ((x >> 8) & ((1 << 3) - 1))
    #define APIC_VECTOR_DS      1 << 12
    #define APIC_VECTOR_RIR     1 << 14
    #define APIC_VECTOR_TGM     1 << 15 //Trigger mode; 0 = Edge Triggered, 1 = Level triggered
    #define APIC_VECTOR_MASK    1 << 16
#define APIC_ERROR_VECTOR       0x370
#define APIC_TIMER_INIT_COUNT   0x380
#define APIC_TIMER_CURR_COUNT   0x390
#define APIC_TIMER_DIV_CONF     0x3E0
#define APIC_FEATURES           0x400
    #define x2APIC_FEATURES_EXTENDED_LVT_COUNT(x)   ((x >> 16) & ((1 << 8) - 1))
    #define x2APIC_FEATURES_EXTENDED_ID             1 << 2
    #define x2APIC_FEATURES_SPECIFIC_EOI            1 << 1
    #define x2APIC_FEATURES_INT_ENABLE_REGISTER     1 << 0
#define APIC_CONTROL            0x410
#define APIC_SEOI               0x420
//#define APIC_IER [0x480-0x4F0]; Interrupt Enable Registers
//#define APIC_LVT [0x500-0x530]; Extended interrupt [3:0] LVT Registers

#define xAPIC_to_x2APIC(offset) (0x800 + offset / 0x10)

#define APIC_SELF (X86_CPU_get_self()->apic)

// xAPIC register manipulation //

u32 xAPIC_read_register(u16 offset) {
    return *(volatile u32*)(APIC_SELF->baseAddress + offset);
}

void xAPIC_write_register(u16 offset, u32 value) {
    *(volatile u32*)(APIC_SELF->baseAddress + offset) = value;
    return;
}

//////////////////////////////////

// x2APIC register manipulation //

u32 x2APIC_read_register(u16 offset) {
    return X86_CPU_rdmsr(xAPIC_to_x2APIC(offset));
}

void x2APIC_write_register(u16 offset, u32 value) {
    X86_CPU_wrmsr(xAPIC_to_x2APIC(offset), value);
    return;
}

//////////////////////////////////

u32 xAPIC_get_id(void) {
    return APIC_SELF->read_register(APIC_ID) >> 24; //only higher 8 bits contain the ID
}

u32 x2APIC_get_id(void) {
    return APIC_SELF->read_register(APIC_ID);
}

//IRQn is unused
void APIC_send_eoi(u8 IRQn) {
    APIC_SELF->write_register(APIC_EOI, 0);
}

bool X86_APIC_init() {
    u32 unused = 0, ecx = 0, edx = 0;
    X86_CPU_cpuid(1, &unused, &unused, &ecx, &edx);
    if(!(edx & CPUID_APIC_SUPPORTED)) return false; //APIC is not supported; not going to happen since we only boot on 64bit cpus anyways

    X86_CPU_cli();

    struct X86_APIC_t* apic = (struct X86_APIC_t*)kalloc(sizeof(struct X86_APIC_t));
    apic->baseAddress = 0xFEE00000; //Address should have been set as UC through MTRRs by the bios.
    MMU_map_page(X86_CPU_get_self()->address_space->CR3, (void*)apic->baseAddress, (void*)apic->baseAddress, MMU_PAGE_4K, MMU_FLAG_NX | MMU_FLAG_GLOBAL | MMU_FLAG_RW | MMU_FLAG_PRESENT, 0);
    apic->send_eoi          = &APIC_send_eoi;
    apic->read_register     = &xAPIC_read_register;
    apic->write_register    = &xAPIC_write_register;
    apic->get_id            = &xAPIC_get_id;

    //Map address into address space and enable the lapic + other things

    X86_CPU_wrmsr(MSR_APIC_BASE_ADDR_REGISTER, MSR_APIC_BASE_ADDR(apic->baseAddress) | MSR_APIC_ENABLE);

    if(apic->read_register(APIC_FEATURES) & x2APIC_FEATURES_EXTENDED_ID) apic->write_register(APIC_CONTROL, x2APIC_FEATURES_EXTENDED_ID);
    //if(apic->read_register(APIC_FEATURES) & x2APIC_FEATURES_SPECIFIC_EOI) lapic->write_register(APIC_CONTROL, x2APIC_FEATURES_SPECIFIC_EOI);

    if(ecx & CPUID_x2APIC_SUPPORTED && ecx & apic->read_register(APIC_VERSION) & APIC_VERSION_x2APIC_PRESENT) {
        apic->baseAddress    = 0;
        apic->read_register  = &x2APIC_read_register;
        apic->write_register = &x2APIC_write_register;
        apic->get_id         = &x2APIC_get_id;

        X86_CPU_wrmsr(MSR_APIC_BASE_ADDR_REGISTER, MSR_X2APIC_ENABLE | MSR_APIC_ENABLE); //Since x2APIC is available, enable it by writing x2APIC_enable and APIC_enable at the same time
    }

    if((u64)APIC_SELF != (u64)apic) kfree(APIC_SELF);
    X86_CPU_get_self()->apic = (struct X86_APIC_t*)apic;
    apic->ID = apic->get_id();

    //TODO: Possibly set LINT0 and LINT1?

    apic->write_register(APIC_SPURIOUS, APIC_ASE | 0xFF); //Set the spurious interrupt at entry 255

    X86_CPU_sti();
    
    return true;
}