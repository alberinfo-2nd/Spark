#ifndef LAPIC_H
#define LAPIC_H

#include <types.h>

struct X86_APIC_t {
    u32 ID;
    u64 baseAddress;

    //Some functions are restricted to X86_APIC_internal_t in apic.c
    void (*send_eoi)(void);
};

bool X86_APIC_init(void);

#endif