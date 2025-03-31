#include <timer/tsc.h>
#include <arch/AMD64/cpu/cpu.h>
#include <timer/pit.h>
#include <timer/timer.h>

#define CPUID_TSC               1 << 4

#define CR4_TSD 1 //2nd bit in CR4

static u32 TSC_freq = 0; //In hz

bool TIMER_TSC_init() {
    u32 edx = 0, unused = 0;
    X86_CPU_cpuid(1, &unused, &unused, &unused, &edx);
    if(edx & CPUID_TSC) {
        X86_CPU_set_cr4_bit(CR4_TSD);

        u32 eax = 0, ebx = 0, ecx = 0;
        X86_CPU_cpuid(0x15, &eax, &ebx, &ecx, &unused);
        TSC_freq = ecx * ebx/eax;

        TIMER_set_timestamp_source(TIMER_TYPE_TSC);
        return true; //No more questions asked.
    }

    // If the TSC is not supported, then enable the PIT as a clock source and return false
    TIMER_set_timestamp_source(TIMER_TYPE_PIT);

    X86_CPU_cli();

    TIMER_PIT_set_mode(PIT_CH0, PIT_MODE_rate_generator);
    TIMER_PIT_set_freq(PIT_CH0, 10000); //Tick at 10khz if the TSC is unsupported (tick every 0.1ms)
    //If the TSC is not invariant, then its problematic to use; A change in p-state or c-state would alter timekeeping, which is not ideal
    
    X86_CPU_sti();
    return false;
}

u64 TIMER_TSC_get_timestamp() {
    u64 rdtsc_val = 0;
    asm volatile("rdtsc" : : "r" (rdtsc_val) :);
    return rdtsc_val / TSC_freq;
}