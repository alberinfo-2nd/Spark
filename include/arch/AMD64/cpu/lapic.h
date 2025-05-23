#ifndef LAPIC_H
#define LAPIC_H

#include <types.h>

bool X86_LAPIC_init(void);
u8 X86_LAPIC_get_apic_id(void);

#endif