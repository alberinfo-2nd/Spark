#ifndef LAPIC_H
#define LAPIC_H

#include <types.h>

struct X86_APIC_t {
    u32 ID;
    u64 baseAddress;

    u32 (*read_register)(u16 offset);
    void (*write_register)(u16 offset, u32 value);
    u32 (*get_id)(void);
    void (*send_eoi)(void);
};

bool X86_APIC_init(void);

#endif