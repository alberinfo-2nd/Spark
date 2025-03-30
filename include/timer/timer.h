#ifndef TIMER_H
#define TIMER_H

#include <types.h>

void TIMER_init(void);
void TIMER_disable(u8 timer_type); //Disables a specific timer. i.e, if LAPIC with TSC deadline is available, TSC is disabled (since it has to provide for the lapic)
void TIMER_sleep(u32 time); //ms
void TIMER_nano_sleep(u64 time);
u64 TIMER_get_boot_timestamp(); //Get how much time passed since boot; Will try to use the TSC by default
u64 TIMER_get_time_since(u64 previous_timestamp);

#endif