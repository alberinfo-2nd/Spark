#include <kernel/timer/timer.h>
#include <kernel/timer/pit.h>
#include <kernel/timer/lapic.h>
#include <kernel/timer/tsc.h>

struct timer_t {
    u8 timer_type;
    bool scanned;
    bool supported;
    bool active;

    //Functions
    bool(*init)(void);
    void(*irq_handler)(void);
    struct TIMER_Timestamp_t (*timestamp)(void);
    void (*sleep)(u64);
};

struct clock_sources_t {
    u8 timestamp;
    u8 sleep;
};

static struct clock_sources_t clock_sources = {0, 0};

struct timer_t timers[5] = {
    {TIMER_TYPE_PIT, false, false, false, &TIMER_PIT_init, &TIMER_PIT_irq_handler, NULL, &TIMER_PIT_sleep},
    {TIMER_TYPE_APIC, false, false, false, &TIMER_APIC_init, &TIMER_APIC_irq_handler, NULL, &TIMER_APIC_sleep},
    {TIMER_TYPE_TSC, false, false, false,  &TIMER_TSC_init, NULL, &TIMER_TSC_get_timestamp, NULL},
    {TIMER_TYPE_APIC_TSC, false, false, false, NULL, NULL, NULL, NULL},
    {TIMER_TYPE_HPET, false, false, false, NULL, NULL, NULL, NULL},
};

//TODO: Make this an actually good sleep system that supports processes and multiple cores.
static bool sleeping = false;

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
    if(timers[timer_type].timestamp == NULL) return;
    clock_sources.timestamp = timer_type;
}

void TIMER_set_sleep_source(u8 timer_type) {
    if(timer_type > TIMER_TYPE_HPET) return;
    if(timers[timer_type].sleep == NULL) return;
    clock_sources.sleep = timer_type;
}

void TIMER_irq_handler(u8 IRQn) {
    for(int i = TIMER_TYPE_PIT; i <= TIMER_TYPE_HPET; i++) {
        if(!timers[i].active) continue;
        if(timers[i].irq_handler == NULL) continue;
        timers[i].irq_handler();
    }
}

struct TIMER_Timestamp_t TIMER_get_boot_timestamp(void) {
    if(timers[clock_sources.timestamp].timestamp == NULL) return (struct TIMER_Timestamp_t){0};
    return timers[clock_sources.timestamp].timestamp();
}

//In milliseconds
void TIMER_sleep(u32 time) {
    //1e6 milliseconds in a nanosecond
    timers[clock_sources.sleep].sleep((u64)time * 1000000);
    return;
}

void TIMER_nano_sleep(u64 time) {
    timers[clock_sources.sleep].sleep(time);
    return;
}