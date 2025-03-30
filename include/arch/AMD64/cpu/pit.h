#ifndef PIT_H
#define PIT_H

#include <types.h>

bool TIMER_PIT_init(void);
void TIMER_PIT_set_reload_register(u8 channel, u16 value);

#endif