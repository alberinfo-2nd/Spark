#include "types.h"
#include <debug/serial.h>
#include <arch/AMD64/cpu/ports.h>

#define SERIAL_COM1 0x3F8

#define SERIAL_INT_ENABLE 1 //send 1 to COM1 to enable IRQs

//LCR = Line Control Register
#define SERIAL_LCR_DATA_SIZE    2 //This equals 7bit transmissions. This is good enough, since we will only ever send ASCII text for debugging
#define SERIAL_LCR_STOP_SHORT   0 << 2
#define SERIAL_LCR_STOP_LONG    1 << 2
#define SERIAL_LCR_PARITY_NONE  0 << 3
#define SERIAL_LCR_PARITY_ODD   1 << 3
#define SERIAL_LCR_PARITY_EVEN  3 << 3
#define SERIAL_LCR_PARITY_MARK  5 << 3
#define SERIAL_LCR_PARITY_SPACE 7 << 3
#define SERIAL_LCR_DLAB_ENABLE  1 << 7 //send to COM1 + 2 to enable DLAB

//IER = Interrupt Enable Register
#define SERIAL_IER_FIFO_ENABLE          1
#define SERIAL_IER_CLEAR_RECEIVE_FIFO   1 << 1
#define SERIAL_IER_CLEAR_TRANSMIT_FIFO  1 << 2
#define SERIAL_IER_DMA_SELECT           1 << 3

//MCR = Modem Control Register
#define SERIAL_MCR_DTR      1 //Data Terminal Ready
#define SERIAL_MCR_RTS      1 << 1 //Rquest to Send

static u16 baud_rate = 1;

//Initializes COM1
void DEBUG_SERIAL_init() {
    outportb(SERIAL_COM1 + 1, SERIAL_INT_ENABLE);
    
    outportb(SERIAL_COM1 + 3, SERIAL_LCR_DLAB_ENABLE);
    outportb(SERIAL_COM1 + 1, baud_rate >> 8);
    outportb(SERIAL_COM1, baud_rate & 0xFF);

    outportb(SERIAL_COM1 + 3, SERIAL_LCR_DATA_SIZE | SERIAL_LCR_STOP_SHORT | SERIAL_LCR_PARITY_NONE);

    outportb(SERIAL_COM1 + 2, SERIAL_IER_FIFO_ENABLE | SERIAL_IER_CLEAR_RECEIVE_FIFO | SERIAL_IER_CLEAR_TRANSMIT_FIFO);

    return;
}

void DEBUG_SERIAL_write(char c) {
    while((inportb(SERIAL_COM1 + 5) & (1 << 5)) == 0); //While Transmitter Holding Register is not empty

    outportb(SERIAL_COM1, c);
}

void DEBUG_SERIAL_write_str(string str) {
    while(str[0]) DEBUG_SERIAL_write(*str++);
}