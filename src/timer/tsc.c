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
        //Ask for the PIT to switch to one-shot

        X86_CPU_set_cr4_bit(CR4_TSD);

        u32 eax = 0, ebx = 0, ecx = 0;
        X86_CPU_cpuid(0x15, &eax, &ebx, &ecx, &unused);
        TSC_freq = ecx * ebx/eax;

        TIMER_set_timestamp_source(TIMER_TYPE_TSC);
        return true; //No more questions asked.
    }

    return false;
}

u64 TIMER_TSC_get_timestamp() {
    u64 rdtsc_val = 0;
    asm volatile("rdtsc" : : "r" (rdtsc_val) :);
    return rdtsc_val / TSC_freq;
}