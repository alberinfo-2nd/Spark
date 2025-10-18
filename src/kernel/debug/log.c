#include <stdarg.h>
#include <kernel/debug/log.h>
#include <kernel/debug/serial.h>
#include <kernel/timer/timer.h>

void DEBUG_log(const char* format, ...) {
    va_list args;
    va_start(args, format);

    //u64 current_time = TIMER_get_boot_timestamp();
    //DEBUG_SERIAL_write_str("%c%s%c", '[', itoa(current_time), ']');

    DEBUG_SERIAL_write_str(format, args);
    DEBUG_SERIAL_write_str("\n", NULL);

    va_end(args);
}