#ifndef LAPIC_H
#define LAPIC_H

#include <types.h>

struct X86_APIC_t {
    u32 ID;
    u64 baseAddress;
    //Functions are restricted to X86_APIC_internal_t in lapic.c
};

bool X86_APIC_init(void);

#endif