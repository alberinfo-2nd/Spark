#include <timer/timer.h>
#include <timer/pit.h>
#include <timer/tsc.h>

struct timer_t {
    u8 timer_type;
    bool scanned;
    bool supported;
    bool active;

    //Functions
    bool(*init)(void);
};

struct clock_sources_t {
    u8 timestamp;
    u8 sleep;
};

static struct clock_sources_t clock_sources = {0, 0};

struct timer_t timers[5] = {
    {TIMER_TYPE_PIT, false, false, false, &TIMER_PIT_init},
    {TIMER_TYPE_TSC, false, false, false,  NULL},
    {TIMER_TYPE_LAPIC, false, false, false, NULL},
    {TIMER_TYPE_LAPIC_TSC, false, false, false, NULL},
    {TIMER_TYPE_HPET, false, false, false, NULL},
};

void TIMER_init() {
    for(int i = 0; i < (int)(sizeof(timers)/sizeof(struct timer_t)); i++) {
        if(timers[i].init == NULL) continue;
        if(timers[i].scanned || timers[i].active) continue; //This means it was already initialized. Works if we want to re-detect our timers, once hpet or lapic are discovered
        timers[i].scanned = true;
        timers[i].supported = timers[i].active = timers[i].init(); //For pit by default its true. For anything else, it depends on the init function to detect the timer and inform that
    }
}

void TIMER_disable(u8 timer_type) {
    if(timer_type > TIMER_TYPE_HPET) return;
    timers[timer_type].active = false;
}

void TIMER_prepare_rediscover(u8 timer_type) {
    if(timer_type > TIMER_TYPE_HPET) return;
    timers[timer_type].scanned = false;
}

void TIMER_set_timestamp_source(u8 timer_type) {
    if(timer_type > TIMER_TYPE_HPET) return;
    clock_sources.timestamp = timer_type;
}

void TIMER_set_sleep_source(u8 timer_type) {
    if(timer_type > TIMER_TYPE_HPET) return;
    clock_sources.sleep = timer_type;
}

void TIMER_irq_handler(u8 IRQn) {
    if(IRQn == 0) {
        //If the pit is enabled it has not been disabled by the HPET or LAPIC. Thus, if the tsc is not enabled the PIT will take care of timekeeping.
        if(clock_sources.timestamp == TIMER_TYPE_PIT) {
            TIMER_PIT_timestamp_increment();
        }
    }
}

u64 TIMER_get_boot_timestamp(void) {
    switch (clock_sources.timestamp) {
        case TIMER_TYPE_PIT:
            return TIMER_PIT_get_timestamp();
        case TIMER_TYPE_TSC:
            return TIMER_TSC_get_timestamp();
        case TIMER_TYPE_LAPIC:
            //Stub
            return 0;
        case TIMER_TYPE_HPET:
            //Stub
            return 0;
    }

    return 0;
}

void TIMER_sleep(u32 time) {
    switch (clock_sources.sleep) {
        case TIMER_TYPE_PIT:
            //TIMER_PIT_sleep()
            break;
        case TIMER_TYPE_LAPIC:
            //Stub
            break;
        case TIMER_TYPE_LAPIC_TSC:
            //Stub
            break;
        case TIMER_TYPE_HPET:
            break;
    }
    return;
}