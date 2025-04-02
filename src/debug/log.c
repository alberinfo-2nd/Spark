#include <stdarg.h>
#include <debug/log.h>
#include <debug/serial.h>
#include <timer/timer.h>

void DEBUG_log(string format, ...) {
    va_list args;
    va_start(args, format);

    //u64 current_time = TIMER_get_boot_timestamp();
    //DEBUG_SERIAL_write_str("%c%s%c", '[', itoa(current_time), ']');

    DEBUG_SERIAL_write_str(format, args);
    DEBUG_SERIAL_write_str((string)"\n");

    va_end(args);
}