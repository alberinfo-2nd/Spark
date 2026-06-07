#include <kernel/timer/tsc.h>
#include <arch/AMD64/cpu/cpu.h>
#include <kernel/timer/pit.h>
#include <kernel/timer/timer.h>

#define CPUID_TSC    1 << 4
#define CPUID_INVTSC 1 << 8
#define CR4_TSD 1 //2nd bit in CR4

#define CALIBRATION_SAMPLE_SLEEP_TIME 25 //Milliseconds slept in order to calibrate measure the TSC frequency
#define CALIBRATION_SAMPLE_COUNT      3  //Take the average of n measurements

static u64 TSC_freq = 0; //In hz

bool TIMER_TSC_init() {
    u32 edx = 0, unused = 0;
    X86_CPU_cpuid(1, &unused, &unused, &unused, &edx);
    if(!(edx & CPUID_TSC)) return false;
    
    edx = 0;
    X86_CPU_cpuid(0x80000007, &unused, &unused, &unused, &edx);
    if(!(edx & CPUID_INVTSC)) return false;

    X86_CPU_set_cr4_bit(CR4_TSD);

    u32 eax = 0, ebx = 0, ecx = 0;
    X86_CPU_cpuid(0x15, &eax, &ebx, &ecx, &unused);
    if(eax == 0 || ebx == 0 || ecx == 0) {
        for(int i = 0; i < CALIBRATION_SAMPLE_COUNT; i++) {
            u64 initialValue = X86_CPU_rdtsc();
            TIMER_sleep(CALIBRATION_SAMPLE_SLEEP_TIME); //Sleep for a few ms to measure how many clocks the counter goes up by
            u64 finalValue = X86_CPU_rdtsc();

            TSC_freq += (finalValue - initialValue) * (1000/CALIBRATION_SAMPLE_SLEEP_TIME);
        }

        TSC_freq /= CALIBRATION_SAMPLE_COUNT;
    } else {
        TSC_freq = ecx * ebx/eax;
    }

    TIMER_set_timestamp_source(TIMER_TYPE_TSC);
    return true;
}

//Pass by value for now....
struct TIMER_Timestamp_t TIMER_TSC_get_timestamp() {
    u64 rdtsc_val = X86_CPU_rdtsc();
    struct TIMER_Timestamp_t timestamp = { 0 };
    timestamp.seconds = rdtsc_val/TSC_freq;
    timestamp.nanoseconds = (rdtsc_val - (timestamp.seconds * TSC_freq)) * 1000000000 / TSC_freq; //Get decimal part of the timestamp and scale to nanoseconds
    return timestamp;
}