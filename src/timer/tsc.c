#include <timer/tsc.h>
#include <arch/AMD64/cpu/cpu.h>
#include <timer/pit.h>

#define CPUID_TSC               1 << 4
#define CPUID_TSC_INVARIANT     1 << 8

#define CR4_TSD 1 //2nd bit in CR4

bool TIMER_TSC_init() {
    u32 edx = 0, unused = 0;
    X86_CPU_cpuid(1, &unused, &unused, &unused, &edx);
    if(!(edx & CPUID_TSC)) goto unsupported; // If the TSC is not supported, then return false

    X86_CPU_cpuid(0x80000007,&unused, &unused, &unused, &edx);
    if(edx & CPUID_TSC_INVARIANT) {
        X86_CPU_set_cr4_bit(CR4_TSD);
        return true; //No more questions asked.
    }

unsupported:
    TIMER_PIT_set_mode(PIT_CH0, PIT_MODE_rate_generator);
    TIMER_PIT_set_freq(PIT_CH0, 10000); //Tick at 10khz if the TSC is unsupported (tick every 0.1ms)
    //If the TSC is not invariant, then its problematic to use; A change in p-state or c-state would alter timekeeping, which is not ideal
    return false;
}