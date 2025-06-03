#include <kernel/mm/pmm.h>
#include <kernel/sync/spinlock.h>
#include <arch/AMD64/cpu/cpu.h>
#include <arch/AMD64/mmu/mmu.h>
#include <arch/AMD64/cpu/lapic.h>
#include <kernel/debug/log.h>

#define align(x, y) ((x+y-1) & ~(y-1)) //Aligns x to the next multiple of y

#define PMM_map_size 4096 //One single entry on the bitmap is 4KiB, same as the smallest MMU entry possible

struct PMM_memory_block_t {
    u64 *bitmap;
    u64 bitmap_size;
    u64 *addr;
    u64 size; //Size in bytes excluding the bitmap
    struct PMM_memory_block_t *next;
};

struct PMM_memory_block_list_t {
    struct PMM_memory_block_t* addr;
    struct PMM_memory_block_list_t* next;
    u32 cpuID;
    struct spinlock_t lock;
};

u8 __starting_memory[16 * 1024] __attribute__((aligned(4096), section(".data"))); //16 KiB of initial memory to allocate a few tables here and there

struct PMM_memory_block_list_t PMM_block_list = {0};

void* PMM_init() {
    if (PMM_block_list.addr == 0) {
        //Initialize first entry
        PMM_block_list.cpuID = X86_LAPIC_get_apic_id(); //Should be zero in the BSP, but just in case
        PMM_add_block(__starting_memory, sizeof(__starting_memory));
        return &PMM_block_list;
    }

    SPINLOCK_acquire_lock(&PMM_block_list.lock); //Prevent the block list from being written by multiple cores at the same time...

    u32 cpuID = X86_LAPIC_get_apic_id();
    struct PMM_memory_block_list_t *block_list = &PMM_block_list;
    while(block_list->next != NULL) block_list = block_list->next;
    //TODO: Allocate memory for the next block list
    block_list->cpuID = cpuID;

    SPINLOCK_acquire_lock(&PMM_block_list.lock);

    return block_list;
}

//Retrieves the block list for the current processor
struct PMM_memory_block_list_t *get_block_list(void) {
    u32 cpuID = X86_LAPIC_get_apic_id();
    struct PMM_memory_block_list_t *block_list = &PMM_block_list;

    while(block_list->cpuID != cpuID && block_list->next != NULL) block_list = block_list->next;

    return block_list->cpuID == cpuID ? block_list : NULL;
}

void PMM_add_block(void* addr, u64 size) {
    struct PMM_memory_block_list_t *block_list = get_block_list();
    if(block_list == NULL) block_list = (struct PMM_memory_block_list_t*)PMM_init();

    u64 bitmap_size = (size-sizeof(struct PMM_memory_block_t)) / PMM_map_size / 8; //How many bytes are needed for the bitmap

    if(MMU_get_address_half(addr) == MMU_addr_lower_half) {
        addr = MMU_make_addr_half(addr, MMU_addr_higher_half);
        if(MMU_get_paddr(NULL, addr) == NULL) {
            //Map 4KiB from the start of addr until the end of the bitmap. covers around 128MiB
            //What do we do if the new address is not aligned to 4KiB? Not handled as of now. TODO
            u8 res = MMU_map_range(NULL, MMU_make_addr_half(addr, MMU_addr_lower_half), addr, bitmap_size, MMU_PAGE_4K, MMU_FLAG_RW | MMU_FLAG_SUPERVISOR | MMU_FLAG_PRESENT);
            if(res != 0) return; //Cannot map address, for now.
        }
    }

    struct PMM_memory_block_t *block = (struct PMM_memory_block_t*)addr;
    block->next = block_list->addr;
    block_list->addr = block;
    
    block->bitmap = (u64*)((u64)addr + sizeof(struct PMM_memory_block_t));
    block->bitmap_size = bitmap_size;
    block->addr = (u64*)MMU_make_addr_half((void*)align((u64)block->bitmap + bitmap_size, PMM_map_size), MMU_addr_lower_half);
    block->size = size - ((u64)block->addr - (u64)block);

    block->next = NULL;
}

//TODO: HANDLE ALIGNMENT, and possibly more things such as placement (for things like DMA, MMIOs and the like)

void* PMM_alloc_aligned(u64 size, u64 alignment) {
    size = align(size, PMM_map_size);

    struct PMM_memory_block_list_t *block_list = get_block_list();
    
    u32 page_count = size / PMM_map_size; //Number of PMM_map_size'd pages to allocate

    for(struct PMM_memory_block_t *block = block_list->addr; block; block = block->next) {
        switch (page_count) {
            case 1:
                for(u64 *bmp = block->bitmap; (u64)bmp <= (u64)block->bitmap + block->bitmap_size; bmp++) {
                    u64 map = ~(*bmp);
                    if(map == 0) continue;
                    
                    u64 map_idx = 0;

                    asm volatile("bsr %1, %0" : "=r" (map_idx) : "r" (map) : "rax", "rbx", "flags");

                    void* addr = (void*)((u64)block->addr + (((u64)bmp - (u64)block->bitmap) * 8 + (64-map_idx-1)) * PMM_map_size);
                    if ((u64)addr >= (u64)block->addr + block->size) break;
                    *bmp |= (u64)1 << map_idx;

                    return addr;
                }
                break;
            default: //More than one page
                return NULL;
                break;
        }
    }

    return NULL;
}

//TODO: HANDLE FREEING


//TODO: HANDLE SMP BLOCK LIST BALOONING