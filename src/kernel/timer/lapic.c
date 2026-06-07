#include <kernel/timer/lapic.h>
#include <arch/AMD64/cpu/cpu.h>
#include <kernel/timer/timer.h>
#include <kernel/timer/pit.h>

#define APIC_TIMER_VECTOR       0x320
#define APIC_TIMER_INIT_COUNT   0x380
#define APIC_TIMER_CURR_COUNT   0x390
#define APIC_TIMER_DIV_CONF     0x3E0

#define APIC_TIMER_MASK 1 << 16

#define CALIBRATION_SAMPLE_SLEEP_TIME 50 //Milliseconds slept in order to calibrate the APIC timer
#define CALIBRATION_SAMPLE_COUNT      3  //Take the average of n measurements

#define APIC_WRITE_REG(reg, val) (X86_CPU_get_self()->apic->write_register(reg, val))
#define APIC_READ_REG(reg) (X86_CPU_get_self()->apic->read_register(reg))

static u64 frequency = 0; //Hz rate of the clock

static volatile bool sleeping = false;

static inline u8 DIVISOR(int x) {
    u8 res = (__builtin_ctz(x)-1) & 7;
    res |= (res & (1 << 2)) << 1;
    return res & ~(1 << 2);
}

bool TIMER_APIC_init() {
    for(int i = 0; i < CALIBRATION_SAMPLE_COUNT; i++) {
        APIC_WRITE_REG(APIC_TIMER_DIV_CONF, DIVISOR(1));
        APIC_WRITE_REG(APIC_TIMER_VECTOR, 0); //Unmask the timer
        APIC_WRITE_REG(APIC_TIMER_INIT_COUNT, 0xFFFFFFFF); //Set initial count

        TIMER_sleep(CALIBRATION_SAMPLE_SLEEP_TIME);

        APIC_WRITE_REG(APIC_TIMER_VECTOR, APIC_TIMER_MASK); //Mask the timer so that IRQs are not generated while the timer is still being initialized
        
        u64 ticksElapsed = 0xFFFFFFFF - APIC_READ_REG(APIC_TIMER_CURR_COUNT);
        frequency += ticksElapsed * 1000/CALIBRATION_SAMPLE_SLEEP_TIME;

        //Not really necessary, but set the initial count to zero just so that no IRQ happens between unmasking and resetting of the initial count on the next iteration
        APIC_WRITE_REG(APIC_TIMER_INIT_COUNT, 0);
    }

    frequency /= CALIBRATION_SAMPLE_COUNT;

    TIMER_PIT_disable();

    APIC_WRITE_REG(APIC_TIMER_INIT_COUNT, 0); //Set initial count to zero, effectively disabling the timer until we sleep.
    APIC_WRITE_REG(APIC_TIMER_VECTOR, 32); //Point the APIC IRQ into IRQ0, entry 32 in IDT and unmask the timer.

    TIMER_set_sleep_source(TIMER_TYPE_APIC);
    TIMER_set_timestamp_source(TIMER_TYPE_APIC);

    return true;
}

void TIMER_APIC_irq_handler() {
    sleeping = false;
}

void TIMER_APIC_sleep(u64 ns) {
    while(ns) {
        u8 divisor = 1;

        //While the maximum sleep time with a certain divisor is less than the time requested and the divisor is valid, increase divisor
        while((0xFFFFFFFFULL*divisor*1000000000) / frequency < ns && divisor < 128) divisor *= 2; //1e9 nanoseconds in a second

        u64 maxSleeptime = (0xFFFFFFFFULL*divisor*1000000000) / frequency;
        u64 sleepTime = ns;
        if(ns > maxSleeptime) sleepTime = maxSleeptime;
        ns -= sleepTime;

        sleeping = true;
        APIC_WRITE_REG(APIC_TIMER_DIV_CONF, DIVISOR(divisor));
        APIC_WRITE_REG(APIC_TIMER_INIT_COUNT, (sleepTime*frequency)/(divisor*1000000000));
        while(sleeping) X86_CPU_hlt();
    }

    //IRQ Handler will be called upon terminal count
    return;
}