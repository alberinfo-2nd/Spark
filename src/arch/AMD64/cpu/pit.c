#include <arch/AMD64/cpu/pit.h>
#include <arch/AMD64/cpu/ports.h>

#define PIT_freq 1193182 //Hz

#define PIT_CH0     0x40
#define PIT_CH1     0x41
#define PIT_CH2     0x42
#define PIT_CMD_REG 0x43

#define PIT_ACCESS_latch    0
#define PIT_ACCESS_lobyte   1
#define PIT_ACCESS_hibyte   2
#define PIT_ACCESS_fullbyte 3

#define PIT_MODE_one_shot           0
#define PIT_MODE_hw_one_shot        1
#define PIT_MODE_rate_generator     2
#define PIT_MODE_square_generator   3
#define PIT_MODE_software_strobe    4
#define PIT_MODE_hw_strobe          5

#define PIT_ENCODING_PLAIN  0 //Plain 16-bit binary values
#define PIT_ENCODING_BCD    1

static u8 current_status[3] = {0,0,0}; //a copy of the value in the command register
static u16 current_divider[3] = {0, 0, 0};

void TIMER_PIT_set_encoding(u8 channel, u8 encoding) { 
    int channel_idx = channel - PIT_CH0;
    current_status[channel_idx] = (current_status[channel_idx] & encoding) | encoding;
    outportb(channel, current_status[channel_idx]);
}

void TIMER_PIT_set_mode(u8 channel, u8 mode) {
    int channel_idx = channel - PIT_CH0;
    current_status[channel_idx] = (current_status[channel_idx] & (mode << 1)) | (mode << 1);
    outportb(channel, current_status[channel_idx]);
}

void TIMER_PIT_set_access_mode(u8 channel, u8 access_mode) {
    int channel_idx = channel - PIT_CH0;
    current_status[channel_idx] = (current_status[channel_idx] & (access_mode << 4)) | (access_mode << 4);
    outportb(channel, current_status[channel_idx]);
}

void TIMER_PIT_set_reload_register(u8 channel, u16 value) {
    int channel_idx = channel - PIT_CH0;
    u8 current_mode = (current_status[channel_idx] & (0b111 << 1)) >> 1;

    //If the current mode for the channel is a rate or square wave generator, then set the divider value. Otherwise, its just a one time use value.
    if(current_mode == PIT_MODE_rate_generator || current_mode == PIT_MODE_square_generator) current_divider[channel_idx] = value;

    outportb(channel, value & 0xFF); //Lobyte
    outportb(channel, value >> 8); //Hibyte
}

void TIMER_PIT_init() {
    //since current_status is initialized to zero, setting the channel for channel 0 is irrelevant
    current_status[1] |= (PIT_CH1-PIT_CH0) << 6;
    current_status[2] |= (PIT_CH2-PIT_CH0) << 6;

    TIMER_PIT_set_encoding(PIT_CH0, PIT_ENCODING_PLAIN);
    TIMER_PIT_set_mode(PIT_CH0, PIT_MODE_one_shot);
    TIMER_PIT_set_access_mode(PIT_CH0, PIT_ACCESS_fullbyte);

    return;
}