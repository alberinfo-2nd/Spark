#ifndef PIT_H
#define PIT_H

#include <types.h>

#define PIT_CH0     0x40
#define PIT_CH1     0x41
#define PIT_CH2     0x42

#define PIT_MODE_one_shot           0
#define PIT_MODE_hw_one_shot        1
#define PIT_MODE_rate_generator     2
#define PIT_MODE_square_generator   3
#define PIT_MODE_software_strobe    4
#define PIT_MODE_hw_strobe          5

bool TIMER_PIT_init(void);
void TIMER_PIT_set_mode(u8 channel, u8 mode);
void TIMER_PIT_set_freq(u8 channel, u32 value);

#endif