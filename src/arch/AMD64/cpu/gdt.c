#include <arch/AMD64/cpu/gdt.h>
#include <arch/AMD64/cpu/cpu.h>
#include <kernel/mm/kalloc.h>

#define GDT_SEGMENT_ACCESSED 1 << 0
#define GDT_SEGMENT_RW 1 << 1
#define GDT_SEGMENT_DIRECTION 1 << 2
#define GDT_SEGMENT_CONFORMING 1 << 2
#define GDT_SEGMENT_EXECUTABLE 1 << 3
#define GDT_SEGMENT_TYPE 1 << 4 // 1 defines code or data. 0 defines a TSS segment entry
#define GDT_SEGMENT_DPL(x) x << 5
#define GDT_DPL_KERNEL GDT_SEGMENT_DPL(0)
#define GDT_DPL_SUPERVISOR0 GDT_SEGMENT_DPL(1)
#define GDT_DPL_SUPERVISOR1 GDT_SEGMENT_DPL(2)
#define GDT_DPL_USER GDT_SEGMENT_DPL(3)
#define GDT_SEGMENT_PRESENT 1 << 7

#define GDT_SEGMENT_LMODE_FLAG 1 << 5

struct GDTR_t {
    u16 limit;
    u64 offset; //Linear address of GDT
} __attribute__((packed));

struct GDT_Segment_t {
    u16 limit; // 0-15 limit. Ignored in long mode
    u16 base; // 16-31 base. Ignored in long mode
    u8 ignored_2; // 32-39 base. Ignored in long mode
    u8 access; // Access permission for the segment
    u8 flags; // lower 4 bits are limit, ignored. higher 4 bits are flags.
    u8 ignored_3; //56-63 base. Ignored in long mode.
} __attribute__((packed));

struct GDT_t {
    // 0x0: Null Descriptor
    // 0x8: Kernel Code Descriptor
    // 0x10: Kernel Data Descriptor
    // 0x18: User Code Descriptor
    // 0x20: User Data Descriptor
    // 0x28: TSS
    struct GDT_Segment_t entries[6];
    struct GDTR_t ptr;
    //TODO: TSSHIGHBITS
    //TODO: TSS
} __attribute__((packed)) __attribute__((aligned(0x20))); //Align to Doubleword (32-bits)

void X86_GDT_setup(struct GDT_t* GDT) {
    GDT->entries[0] = (struct GDT_Segment_t){ 0 }; //Null descriptor
    GDT->entries[1] = (struct GDT_Segment_t){ .access = GDT_SEGMENT_EXECUTABLE | GDT_SEGMENT_TYPE | GDT_DPL_KERNEL | GDT_SEGMENT_PRESENT, .flags = GDT_SEGMENT_LMODE_FLAG, .limit = 0xFFFF }; //Kernel code descriptor
    GDT->entries[2] = (struct GDT_Segment_t){ .access = GDT_SEGMENT_RW | GDT_SEGMENT_TYPE | GDT_DPL_KERNEL | GDT_SEGMENT_PRESENT, .limit = 0xFFFF }; //Kernel data descriptor
    GDT->entries[3] = (struct GDT_Segment_t){ .access = GDT_SEGMENT_EXECUTABLE | GDT_SEGMENT_TYPE | GDT_DPL_USER | GDT_SEGMENT_PRESENT, .flags = GDT_SEGMENT_LMODE_FLAG, .limit = 0xFFFF }; //User code descriptor
    GDT->entries[4] = (struct GDT_Segment_t){ .access = GDT_SEGMENT_RW | GDT_SEGMENT_TYPE | GDT_DPL_USER | GDT_SEGMENT_PRESENT, .limit = 0xFFFF }; //User data descriptor
    //TSS

    GDT->ptr.offset = (u64)GDT->entries;
    GDT->ptr.limit = sizeof(GDT->entries)-1;

    return;
}

void X86_GDT_install(void) {
    struct GDT_t* GDT = kalloc(sizeof(struct GDT_t));
    X86_GDT_setup(GDT);
    X86_CPU_get_self()->gdt = GDT;

    asm volatile("lgdt (%0)" : : "r" (&GDT->ptr));
    return;
}