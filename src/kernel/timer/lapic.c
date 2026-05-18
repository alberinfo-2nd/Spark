#include <kernel/timer/lapic.h>
#include <arch/AMD64/cpu/cpu.h>
#include <kernel/timer/timer.h>
#include <kernel/timer/pit.h>

#define APIC_TIMER_VECTOR       0x320
#define APIC_TIMER_INIT_COUNT   0x380
#define APIC_TIMER_CURR_COUNT   0x390
#define APIC_TIMER_DIV_CONF     0x3E0

#define APIC_TIMER_MASK 1 << 16

//#define DIVISOR(x) (( __builtin_ctz(x)-1) & 7)

static u64 frequency = 0; //How many ticks for 1 Microsecond using divisor 1

static bool sleeping = false;

static inline u8 DIVISOR(int x) {
    u8 res = (__builtin_ctz(x)-1) & 7;
    res |= (res & (1 << 2)) << 1;
    return res & ~(1 << 2);
}

bool TIMER_APIC_init() {
    X86_CPU_get_self()->apic->write_register(APIC_TIMER_DIV_CONF, DIVISOR(4));
    X86_CPU_get_self()->apic->write_register(APIC_TIMER_INIT_COUNT, 0xFFFFFFFF);
    X86_CPU_get_self()->apic->write_register(APIC_TIMER_VECTOR, 0);

    TIMER_sleep(250); //Sleep for 250ms

    X86_CPU_get_self()->apic->write_register(APIC_TIMER_VECTOR, APIC_TIMER_MASK);
    u64 ticksElapsed = 0xFFFFFFFF - X86_CPU_get_self()->apic->read_register(APIC_TIMER_CURR_COUNT);

    frequency = ticksElapsed/(250 * 1000) * 4; //We used a divisor of 4 to calibrate the timer

    TIMER_PIT_disable();

    X86_CPU_get_self()->apic->write_register(APIC_TIMER_INIT_COUNT, 0); //Set initial count to zero, effectively disabling the timer until we sleep.
    X86_CPU_get_self()->apic->write_register(APIC_TIMER_VECTOR, 32); //Point the APIC IRQ into IRQ0, entry 32 in IDT

    TIMER_set_sleep_source(TIMER_TYPE_APIC);
    TIMER_set_timestamp_source(TIMER_TYPE_APIC);
    return true;
}

void TIMER_APIC_irq_handler() {
    sleeping = false;
}

void TIMER_APIC_sleep(u64 ns) {
    u64 timeInMicroseconds = ns/1000;
    u8 divisor = 1;
    while(0xFFFFFFFF / (frequency * divisor) < timeInMicroseconds && divisor < 128) divisor *= 2;

    //TODO: What if even the max divisor is not enough for the requested sleeping time?

    sleeping = true;
    X86_CPU_get_self()->apic->write_register(APIC_TIMER_DIV_CONF, DIVISOR(divisor));
    X86_CPU_get_self()->apic->write_register(APIC_TIMER_INIT_COUNT, frequency * timeInMicroseconds/divisor);

    while(sleeping) X86_CPU_hlt();

    //IRQ Handler will be called upon terminal count
    return;
}