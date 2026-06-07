#ifndef TSC_H
#define TSC_H

#include <types.h>

bool TIMER_TSC_init(void);
struct TIMER_Timestamp_t TIMER_TSC_get_timestamp(void);

#endif