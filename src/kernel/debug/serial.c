#include <types.h>
#include <kernel/debug/serial.h>
#include <arch/AMD64/cpu/ports.h>

#define SERIAL_COM1 0x3F8

#define SERIAL_INT_DISABLE 0
#define SERIAL_INT_ENABLE  1 //send 1 to COM1 to enable IRQs

//LCR = Line Control Register
#define SERIAL_LCR_DATA_SIZE    3 //8-bit transmission size.
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
#define SERIAL_ITL_14Bytes              0b11 << 6

//MCR = Modem Control Register
#define SERIAL_MCR_DTR      1 //Force data Terminal Ready
#define SERIAL_MCR_RTS      1 << 1 //Force request to Send
#define SERIAL_MCR_AUX1     1 << 2
#define SERIAL_MCR_AUX2     1 << 3

//LSR = Line Status Register
#define SERIAL_LSR_EHR      (1 << 5) //Empty Transmitter Holding Register

static u16 baud_rate = 1;

void DEBUG_SERIAL_write(char c);

//Initializes COM1
void DEBUG_SERIAL_init() {
    outportb(SERIAL_COM1 + 1, SERIAL_INT_DISABLE);
    
    outportb(SERIAL_COM1 + 3, SERIAL_LCR_DLAB_ENABLE);
    
    outportb(SERIAL_COM1, baud_rate & 0xFF);
    outportb(SERIAL_COM1 + 1, baud_rate >> 8);

    outportb(SERIAL_COM1 + 3, SERIAL_LCR_DATA_SIZE | SERIAL_LCR_STOP_SHORT | SERIAL_LCR_PARITY_NONE);

    outportb(SERIAL_COM1 + 2, SERIAL_IER_FIFO_ENABLE | SERIAL_IER_CLEAR_RECEIVE_FIFO | SERIAL_IER_CLEAR_TRANSMIT_FIFO | SERIAL_ITL_14Bytes);

    outportb(SERIAL_COM1 + 4, SERIAL_MCR_DTR | SERIAL_MCR_RTS | SERIAL_MCR_AUX1 | SERIAL_MCR_AUX2);

    return;
}

void DEBUG_SERIAL_write(char c) {
    while((inportb(SERIAL_COM1 + 5) & SERIAL_LSR_EHR) == 0); //While Transmitter Holding Register is not empty

    outportb(SERIAL_COM1, c);
}

void DEBUG_SERIAL_write_str(const char* format, va_list args) {
    while(*format) {
        switch (*format) {
            case '%':
                format++;
                switch (*format) {
                    case '%': {
                        DEBUG_SERIAL_write('%');
                        break;
                    } case 'c': {
                        DEBUG_SERIAL_write((char)va_arg(args, int));
                        break;
                    } case 's': {
                        DEBUG_SERIAL_write_str(va_arg(args, void*), args);
                        break;
                    } case 'X':
                    case 'x': {
                        DEBUG_SERIAL_write_str("0x\0", args);

                        u8 mask_move_count = 64 - 4; //From the start move everything except the most significant nibble

                        u64 mask = 0xF000000000000000;
                        u64 arg = (u64)va_arg(args, u64);
                        // Remove leading zeroes
                        // while (!(arg & mask) && mask != 0xF) {
                        //     mask >>= 4;
                        //     mask_move_count -= 4;
                        // }
                        while (mask) {
                            char c = (arg & mask) >> mask_move_count;
                            char letterIdx = *format == 'x' ? 'a' : 'A';
                            if(c < 0xA) c += '0';
                            else c = c - 0xA + letterIdx;

                            DEBUG_SERIAL_write(c);

                            mask >>= 4;
                            mask_move_count -= 4;
                        }
                        break;
                    } case 'i': {
                        u64 arg = (u64)va_arg(args, u64);
                        u64 reverseArg = 0;
                        u64 digits = 0;

                        do {
                            reverseArg *= 10;
                            reverseArg += arg % 10;
                            arg /= 10;
                            digits++;
                        } while(arg);

                        do {
                            char c = (reverseArg % 10) + '0';
                            DEBUG_SERIAL_write(c);
                            reverseArg /= 10;
                        } while(--digits);

                    //Other cases should also be handled
                    } case '\0': {
                        break;

                    } default: {
                        break;
                    }
                }
                break;
            default:
                DEBUG_SERIAL_write(*format);
        }

        format++;
    }

    return;
}