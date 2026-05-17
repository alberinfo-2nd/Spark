#include <kernel/mm/ekalloc.h>
#include <kernel/mm/pmm.h>
#include <arch/AMD64/mmu/mmu.h>

//TODO: Maybe give back the data allocated to the normal kernel allocator? problem is early kernel allocator has no metadata, so it is a bit hard. And probably useless

#define PMM_map_size 4096

void* ptr = 0;
u32 remaining_space = 0;
bool finished_lock = false; //If set to true, then kill the early kernel allocator.

//Grab a new page from the PMM
void ekalloc_expand(void) {
    remaining_space = PMM_map_size;
    ptr = MMU_make_addr_half(PMM_alloc_aligned(PMM_map_size, 0), MMU_addr_kernel_half); //TODO: Maybe the address is not mapped into the address space!!
}

void* ekalloc(u32 size) {
    if(finished_lock) return NULL;

    if(size > PMM_map_size) {
        //Bypass the allocator and redirect the request straight to the PMM.
        void* rptr = PMM_alloc_aligned(size, 0);
        return MMU_make_addr_half(rptr, MMU_addr_kernel_half);
    }

    if(remaining_space < size) ekalloc_expand(); //Very wasteful of memory

    void* rptr = ptr;
    ptr = (void*)((u64)ptr + size);
    remaining_space -= size;

    return rptr;
}

void ekalloc_finish(void) {
    finished_lock = true;
}