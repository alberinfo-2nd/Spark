#include <kernel/timer/tsc.h>
#include <arch/AMD64/cpu/cpu.h>
#include <kernel/timer/pit.h>
#include <kernel/timer/timer.h>

#define CPUID_TSC               1 << 4

#define CR4_TSD 1 //2nd bit in CR4

static u32 TSC_freq = 0; //In hz

bool TIMER_TSC_init() {
    u32 edx = 0, unused = 0;
    X86_CPU_cpuid(1, &unused, &unused, &unused, &edx);
    if(edx & CPUID_TSC) {
        //Ask for the PIT to switch to one-shot

        X86_CPU_set_cr4_bit(CR4_TSD);

        u32 eax = 0, ebx = 0, ecx = 0;
        X86_CPU_cpuid(0x15, &eax, &ebx, &ecx, &unused);

        if(eax == 0 || ecx == 0) {
            u64 initialValue = X86_CPU_rdtsc();
            TIMER_sleep(10); //Sleep for 10ms to measure how many clocks the counter goes up by
            u64 finalValue = X86_CPU_rdtsc();

            TSC_freq = (finalValue - initialValue) * (1000/10);
        } else {
            TSC_freq = ecx * ebx/eax;
        }

        TIMER_set_timestamp_source(TIMER_TYPE_TSC);
        return true; //No more questions asked.
    }

    return false;
}

u64 TIMER_TSC_get_timestamp() {
    u64 rdtsc_val = X86_CPU_rdtsc();
    return rdtsc_val / TSC_freq;
}