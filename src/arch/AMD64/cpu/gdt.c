#include <arch/AMD64/cpu/gdt.h>
#include <arch/AMD64/cpu/cpu.h>
#include <kernel/mm/kalloc.h>
#include <string.h>

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

#define GDT_SEGMENT_LDT         0b0010
#define GDT_SEGMENT_TSS_AVL     0b1001
#define GDT_SEGMENT_TSS_BSY     0b1011
#define GDT_SEGMENT_CALL_GATE   0b1100
#define GDT_SEGMENT_INT_GATE    0b1110
#define GDT_SEGMENT_TRAP_GATE   0b1111

#define get_bits(x, l, h) (((u64)x >> l) & ((1ULL << (h-l+1)) - 1))

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

struct System_Segment_t {
    u16 limit;
    u16 base0; // 0-15 base
    u8 base1; // 16-23 base
    u8 access;
    u8 flags; // lowest 4 bits are limit. then bit 4 is available, 5-6 are reserved and 7 is granularity
    u8 base2; // 24-31 base
    u32 base3; // 32-63 base
    u32 : 32; // reserved, some bits MBZ
} __attribute__((packed));

struct GDT_t {
    // 0x0: Null Descriptor
    // 0x8: Kernel Code Descriptor
    // 0x10: Kernel Data Descriptor
    // 0x18: User Code Descriptor
    // 0x20: User Data Descriptor
    // 0x28: TSS
    struct GDT_Segment_t entries[5];
    struct System_Segment_t TSS;
    //Possibly more space for more descriptors?
    struct GDTR_t ptr;
} __attribute__((packed)) __attribute__((aligned(0x20))); //Align to Doubleword (32-bits)

struct TSS_t {
    u32 : 32; //Reserved offset 0x0-0x4
    u32 RSP[3][2]; //RSPn low[31:0]-high[63:32]
    u64 : 64; //Reserved offset 0x1C-0x24
    u32 IST[7][2]; //ISTn low[31:0]-high[63:32]
    u64 : 64; //Reserved offset 0x5C-0x64
    u16 : 16; //Reserved offset 0x64-0x74
    u16 IOMap_BA; //IO Map Base Address
} __attribute__((packed)) __attribute((aligned(0x20)));

void X86_GDT_setup(struct GDT_t* GDT) {
    GDT->entries[0] = (struct GDT_Segment_t){ 0 }; //Null descriptor
    GDT->entries[1] = (struct GDT_Segment_t){ .access = GDT_SEGMENT_EXECUTABLE | GDT_SEGMENT_TYPE | GDT_DPL_KERNEL | GDT_SEGMENT_PRESENT, .flags = GDT_SEGMENT_LMODE_FLAG, .limit = 0xFFFF }; //Kernel code descriptor
    GDT->entries[2] = (struct GDT_Segment_t){ .access = GDT_SEGMENT_RW | GDT_SEGMENT_TYPE | GDT_DPL_KERNEL | GDT_SEGMENT_PRESENT, .limit = 0xFFFF }; //Kernel data descriptor
    GDT->entries[3] = (struct GDT_Segment_t){ .access = GDT_SEGMENT_EXECUTABLE | GDT_SEGMENT_TYPE | GDT_DPL_USER | GDT_SEGMENT_PRESENT, .flags = GDT_SEGMENT_LMODE_FLAG, .limit = 0xFFFF }; //User code descriptor
    GDT->entries[4] = (struct GDT_Segment_t){ .access = GDT_SEGMENT_RW | GDT_SEGMENT_TYPE | GDT_DPL_USER | GDT_SEGMENT_PRESENT, .limit = 0xFFFF }; //User data descriptor
    
    struct TSS_t* TSS = kalloc(sizeof(struct TSS_t));
    memset(TSS, 0, sizeof(struct TSS_t));
    GDT->TSS = (struct System_Segment_t){ .limit = sizeof(struct TSS_t)-1, .base0 = get_bits(TSS, 0, 15), .base1 = get_bits(TSS, 16, 23), .base2 = get_bits(TSS, 24, 31), .base3 = get_bits(TSS, 32, 63), .access = GDT_DPL_KERNEL | GDT_SEGMENT_PRESENT | GDT_SEGMENT_TSS_AVL}; //Flags to 0xF expands the higher 4-bit area of segment limit

    GDT->ptr.offset = (u64)GDT->entries;
    GDT->ptr.limit = sizeof(struct GDT_t)-sizeof(struct GDTR_t)-1;

    return;
}

void X86_GDT_install(void) {
    struct GDT_t* GDT = kalloc(sizeof(struct GDT_t));
    memset(GDT, 0, sizeof(struct GDT_t));
    X86_GDT_setup(GDT);

    asm volatile("lgdt (%0)" : : "r" (&GDT->ptr));
    asm volatile("ltr %0" : : "r" ((u64)&GDT->TSS - (u64)GDT));
    return;
}

struct GDTR_t X86_GDT_get_ptr(void) {
    struct GDTR_t GDT = {};
    asm volatile("sgdt %0" : : "m" (GDT) : "memory");
    return GDT;
}