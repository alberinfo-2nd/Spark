#ifndef TSC_H
#define TSC_H

#include <types.h>

bool TIMER_TSC_init(void);
u64 TIMER_TSC_get_timestamp(void);

#endif