#include <timer/pit.h>
#include <arch/AMD64/cpu/ports.h>

#define PIT_freq 1193182 //Hz
#define PIT_count_quantum 838 //calculated from the pit frequency; each tick in the countdown is 838ns (applies only when using one-shot mode)

#define PIT_CMD_REG 0x43

#define PIT_ACCESS_latch    0
#define PIT_ACCESS_lobyte   1
#define PIT_ACCESS_hibyte   2
#define PIT_ACCESS_fullbyte 3

#define PIT_ENCODING_PLAIN  0 //Plain 16-bit binary values
#define PIT_ENCODING_BCD    1

static u8 current_status[3] = {0,0,0}; //a copy of the value in the command register
static u16 current_divider[3] = {0, 0, 0};

static u64 wall_clock = 0; //Used for timekeeping with the PIT. strongly discouraged.

void TIMER_PIT_set_encoding(u8 channel, u8 encoding) { 
    int channel_idx = channel - PIT_CH0;
    current_status[channel_idx] = (current_status[channel_idx] & ~1) | encoding;
    outportb(PIT_CMD_REG, current_status[channel_idx]);
}

void TIMER_PIT_set_mode(u8 channel, u8 mode) {
    int channel_idx = channel - PIT_CH0;
    current_status[channel_idx] = (current_status[channel_idx] & ~(0b111 << 1)) | (mode << 1);
    outportb(PIT_CMD_REG, current_status[channel_idx]);
}

void TIMER_PIT_set_access_mode(u8 channel, u8 access_mode) {
    int channel_idx = channel - PIT_CH0;
    current_status[channel_idx] = (current_status[channel_idx] & ~(0b11 << 4)) | (access_mode << 4);
    outportb(PIT_CMD_REG, current_status[channel_idx]);
}

void TIMER_PIT_set_reload_register(u8 channel, u16 value) {
    outportb(channel, value & 0xFF); //Lobyte
    outportb(channel, value >> 8); //Hibyte
}

//If current mode is one shot, then value represents time until interrupt in microseconds. For generators, its frequency in hz
void TIMER_PIT_set_freq(u8 channel, u32 value) {
    int channel_idx = channel - PIT_CH0;
    u8 current_mode = (current_status[channel_idx] & (0b111 << 1)) >> 1;

    //If the current mode for the channel is a rate or square wave generator, then set the divider value. Otherwise, its just a one time use value.
    if(current_mode == PIT_MODE_rate_generator || current_mode == PIT_MODE_square_generator) {
        u16 div = PIT_freq / (value & 0xFFFF);
        current_divider[channel_idx] = div;
        TIMER_PIT_set_reload_register(channel, div);
        return;
    }

    if(value > PIT_count_quantum * 65536 / 1000) value = PIT_count_quantum * 65536 / 1000; //Maximum interval is ~55ms
    u16 ticks_until_irq = value * 1000 / PIT_count_quantum;
    TIMER_PIT_set_reload_register(channel, ticks_until_irq);
}

bool TIMER_PIT_init() {
    wall_clock = 0;

    //since current_status is initialized to zero, setting the channel for channel 0 is irrelevant
    current_status[1] |= (PIT_CH1-PIT_CH0) << 6;
    current_status[2] |= (PIT_CH2-PIT_CH0) << 6;

    TIMER_PIT_set_encoding(PIT_CH0, PIT_ENCODING_PLAIN);
    TIMER_PIT_set_mode(PIT_CH0, PIT_MODE_one_shot);
    TIMER_PIT_set_access_mode(PIT_CH0, PIT_ACCESS_fullbyte);

    return true;
}

void TIMER_PIT_timestamp_increment() {
    wall_clock += (u64)1e9/(PIT_freq/current_divider[0]);
}

u64 TIMER_PIT_get_timestamp() {
    return wall_clock; //1e9 nanoseconds in a second
}