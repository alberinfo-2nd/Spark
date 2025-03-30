#include <timer/timer.h>
#include <arch/AMD64/cpu/pit.h>

#define TIMER_TYPE_PIT          0
#define TIMER_TYPE_TSC          1
#define TIMER_TYPE_LAPIC        2
#define TIMER_TYPE_LAPIC_TSC    3
#define TIMER_TYPE_HPET         4

struct timer_t {
    u8 timer_type;
    bool supported;
    bool active;
    u8 capabilities;

    //Functions
    bool(*init)();
};

struct timer_t timers[5] = {
    {TIMER_TYPE_PIT, false, false, 0, &TIMER_PIT_init},
    {TIMER_TYPE_TSC, false, false, 0,  NULL},
    {TIMER_TYPE_LAPIC, false, false, 0, NULL},
    {TIMER_TYPE_LAPIC_TSC, false, false, 0, NULL},
    {TIMER_TYPE_HPET, false, false, 0, NULL},
};

void TIMER_init() {
    for(int i = 0; i < sizeof(timers)/sizeof(struct timer_t); i++) {
        if(timers[i].init == NULL) continue;
        if(timers[i].supported == true) continue; //This means it was already initialized. Works if we want to re-detect our timers, once hpet or lapic are discovered
        timers[i].supported = timers[i].active = timers[i].init(); //For pit by default its true. For anything else, it depends on the init function to detect the timer and inform that
    }
}