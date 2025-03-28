#ifndef DEBUG_SERIAL_H
#define DEBUG_SERIAL_H

#include <types.h>

void DEBUG_SERIAL_init(void);
void DEBUG_SERIAL_write(char c);
void DEBUG_SERIAL_write_str(string str);

#endif