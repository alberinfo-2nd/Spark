#include <arch/AMD64/mmu/mmu.h>
#include <arch/AMD64/cpu/cpu.h>

//Stores the pointer to the current cr3 in each thread / core
struct address_spaces_t {
    void* addr;
    struct address_spaces_t* next;
    u32 cpuId;
};

struct address_spaces_t current_address_space;

void MMU_switch_cr3(void *PML4, u32 cpuId) {
    for(struct address_spaces_t* address_space = &current_address_space; address_space; address_space = address_space->next) {
        if(address_space->cpuId != cpuId) continue;

        address_space->addr = PML4;
        break;
    }

    X86_CPU_set_cr3(PML4);
}