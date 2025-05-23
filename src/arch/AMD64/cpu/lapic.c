#include <arch/AMD64/cpu/lapic.h>
#include <arch/AMD64/cpu/cpu.h>

#define APIC_SUPPORTED 1 << 9

bool APIC_enabled = false;

bool X86_LAPIC_init() {
    u32 unused = 0, edx = 0;
    X86_CPU_cpuid(1, &unused, &unused, &unused, &edx);
    if(!(edx & APIC_SUPPORTED)) return false; //APIC is not supported; not going to happen since we only boot on 64bit cpus anyways

    //Map address into address space and enable the lapic + other things

    APIC_enabled = true;

    return true;
}

u8 X86_LAPIC_get_apic_id(void) {
    if(!APIC_enabled) return X86_CPU_get_cpuid();

    //TODO: return actual id
    return 0;
}