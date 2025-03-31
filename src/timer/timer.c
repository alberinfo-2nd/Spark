#include <timer/timer.h>
#include <timer/pit.h>
#include <timer/tsc.h>

#define TIMER_TYPE_PIT          0
#define TIMER_TYPE_TSC          1
#define TIMER_TYPE_LAPIC        2
#define TIMER_TYPE_LAPIC_TSC    3
#define TIMER_TYPE_HPET         4

struct timer_t {
    u8 timer_type;
    bool scanned;
    bool supported;
    bool active;
    u8 capabilities;

    //Functions
    bool(*init)(void);
};

struct timer_t timers[5] = {
    {TIMER_TYPE_PIT, false, false, false, 0, &TIMER_PIT_init},
    {TIMER_TYPE_TSC, false, false, false, 0,  &TIMER_TSC_init},
    {TIMER_TYPE_LAPIC, false, false, false, 0, NULL},
    {TIMER_TYPE_LAPIC_TSC, false, false, false, 0, NULL},
    {TIMER_TYPE_HPET, false, false, false, 0, NULL},
};

void TIMER_init() {
    for(int i = 0; i < (int)(sizeof(timers)/sizeof(struct timer_t)); i++) {
        if(timers[i].init == NULL) continue;
        if(timers[i].scanned) continue; //This means it was already initialized. Works if we want to re-detect our timers, once hpet or lapic are discovered
        timers[i].scanned = true;
        timers[i].supported = timers[i].active = timers[i].init(); //For pit by default its true. For anything else, it depends on the init function to detect the timer and inform that
    }
}

void TIMER_irq_handler(u8 IRQn) {
    if(IRQn == 0) {
        //If the pit is enabled it has not been disabled by the HPET or LAPIC. Thus, if the tsc is not enabled the PIT will take care of timekeeping.
        if(timers[TIMER_TYPE_PIT].active == true && timers[TIMER_TYPE_TSC].active == false) {
            TIMER_PIT_timestamp_increment();
        } else {
            //sth else
        }
    }
}

u64 TIMER_get_boot_timestamp(void) {
    if(timers[TIMER_TYPE_TSC].active) {
        //Stub
        return 0;
    }

    if(timers[TIMER_TYPE_LAPIC].active) {
        //Stub
        return 0;
    }

    if(timers[TIMER_TYPE_HPET].active) {
        //Stub
        return 0;
    }

    return TIMER_PIT_get_timestamp();
}