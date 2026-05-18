#ifndef LAPIC_TIMER_H
#define LAPIC_TIMER_H

#include <types.h>
#include <stdbool.h>

bool TIMER_APIC_init(void);
void TIMER_APIC_irq_handler(void);
void TIMER_APIC_sleep(u64 ns);

#endif