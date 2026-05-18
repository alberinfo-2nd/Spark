#ifndef TIMER_H
#define TIMER_H

#include <types.h>

#define TIMER_TYPE_PIT      0
#define TIMER_TYPE_APIC     1
#define TIMER_TYPE_TSC      2
#define TIMER_TYPE_APIC_TSC 3
#define TIMER_TYPE_HPET     4

void TIMER_init(void);
void TIMER_disable(u8 timer_type); //Disables a specific timer. i.e, if LAPIC with TSC deadline is available, TSC is disabled (since it has to provide for the lapic)
void TIMER_prepare_rediscover(u8 timer_type); //Reenables a specific timer for discovery through TIMER_init
void TIMER_set_timestamp_source(u8 timer_type); //Changes the clock source for the timestamp
void TIMER_set_sleep_source(u8 timer_type); //Same as above but for sleep
void TIMER_sleep(u32 time); //ms
void TIMER_nano_sleep(u64 time);
u64 TIMER_get_boot_timestamp(void); //Get how much time passed since boot in nanosecs; Will try to use the TSC by default
u64 TIMER_get_time_since(u64 previous_timestamp);
void TIMER_irq_handler(u8 IRQn);

#endif