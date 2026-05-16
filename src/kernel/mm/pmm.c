#include <kernel/mm/pmm.h>
#include <kernel/sync/spinlock.h>
#include <arch/AMD64/cpu/cpu.h>
#include <arch/AMD64/mmu/mmu.h>
#include <kernel/debug/log.h>
#include <string.h>

#define align(x, y) ((x+y-1) & ~(y-1)) //Aligns x to the next multiple of y

#define PMM_map_size 4096 //One single entry on the bitmap is 4KiB, same as the smallest MMU entry possible

extern u8 KERNEL_END;

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
        PMM_block_list.cpuID = X86_CPU_get_cpuid(); //Contains the same value as the Local APIC ID at boot.
        PMM_add_block(__starting_memory, sizeof(__starting_memory));
        return &PMM_block_list;
    }

    SPINLOCK_acquire_lock(&PMM_block_list.lock); //Prevent the block list from being written by multiple cores at the same time...

    u32 cpuID = X86_CPU_get_self()->apic->ID;
    struct PMM_memory_block_list_t *block_list = &PMM_block_list;
    while(block_list->next != NULL) block_list = block_list->next;
    //TODO: Allocate memory for the next block list
    block_list->cpuID = cpuID;

    SPINLOCK_acquire_lock(&PMM_block_list.lock);

    return block_list;
}

//Retrieves the block list for the current processor
struct PMM_memory_block_list_t *get_block_list(void) {
    u32 cpuID = 0;
    if(!X86_CPU_self_initialized()) cpuID = X86_CPU_get_cpuid();
    else cpuID = X86_CPU_get_self()->apic->ID;
    struct PMM_memory_block_list_t *block_list = &PMM_block_list;

    while(block_list->cpuID != cpuID && block_list->next != NULL) block_list = block_list->next;

    return block_list->cpuID == cpuID ? block_list : NULL;
}

void PMM_add_block(void* addr, u64 size) {
    if(MMU_get_address_half(addr) != MMU_addr_kernel_half && MMU_make_addr_half(addr, MMU_addr_lower_half) < MMU_make_addr_half(&KERNEL_END, MMU_addr_lower_half)) {
        u64 diff = (u64)(MMU_make_addr_half(&KERNEL_END, MMU_addr_lower_half) - MMU_make_addr_half(addr, MMU_addr_lower_half));
        if(size < diff) return; //Block size is too small
        size -= diff;
        addr += diff;
    }
    //if(size < PMM_map_size*16) return; //Having small memory blocks is uneffective

    struct PMM_memory_block_list_t *block_list = get_block_list();
    if(block_list == NULL) block_list = (struct PMM_memory_block_list_t*)PMM_init();

    u64 bitmap_size = (size-sizeof(struct PMM_memory_block_t)) / PMM_map_size / 8; //How many bytes are needed for the bitmap

    if(MMU_get_address_half(addr) == MMU_addr_lower_half) {
        addr = MMU_make_addr_half(addr, MMU_addr_higher_half);
        if(MMU_get_paddr(NULL, addr) == NULL) {
            //Map 4KiB from the start of addr until the end of the bitmap. covers around 128MiB
            //What do we do if the new address is not aligned to 4KiB? Not handled as of now. TODO
            u8 res = MMU_map_range(NULL, MMU_make_addr_half(addr, MMU_addr_lower_half), addr, align(bitmap_size, MMU_PAGE_4K), MMU_PAGE_4K, MMU_FLAG_RW | MMU_FLAG_SUPERVISOR | MMU_FLAG_PRESENT, 0);
            if(res != 0) return; //Cannot map address, for now.
        }
    }

    for(int i = 0; (u64)((u64*)addr+i) < (u64)addr+bitmap_size+sizeof(struct PMM_memory_block_t); i++) *((u64*)addr+i) = 0;

    struct PMM_memory_block_t *block = (struct PMM_memory_block_t*)addr;
    block->next = block_list->addr;
    block_list->addr = block;
    
    block->bitmap = (u64*)((u64)addr + sizeof(struct PMM_memory_block_t));
    block->bitmap_size = bitmap_size;
    block->addr = (u64*)MMU_make_addr_half((void*)align((u64)block->bitmap + block->bitmap_size, PMM_map_size), MMU_addr_lower_half);
    block->size = (u64)block->addr - (u64)block->bitmap - bitmap_size;

    block->next = NULL;
}

//TODO: HANDLE ALIGNMENT, and possibly more things such as placement (for things like DMA, MMIOs and the like)
void* PMM_alloc_aligned(u64 size, u64 alignment) {
    size = align(size, PMM_map_size);

    struct PMM_memory_block_list_t *block_list = get_block_list();
    
    u32 page_count = size / PMM_map_size; //Number of PMM_map_size'd pages to allocate

    for(struct PMM_memory_block_t *block = block_list->addr; block; block = block->next) {
        if(page_count == 1) {
            for(u64 *bmp = block->bitmap; (u64)bmp <= (u64)block->bitmap + block->bitmap_size; bmp++) {
                u64 map = ~(*bmp);
                if(map == 0) continue;

                u64 map_idx = 0;
                asm volatile("bsr %1, %0" : "=r" (map_idx) : "r" (map) : "rax", "rbx", "flags");

                void* addr = (void*)((u64)block->addr + (((u64)bmp - (u64)block->bitmap) * 8 + (64-map_idx-1)) * PMM_map_size);
                if ((u64)addr+size >= (u64)block->addr + block->size) break; //If the end of the allocation goes over the end of the block, then ignore it and try again.
                *bmp |= (u64)1 << map_idx;

                memset(addr, 0, size); //Probably not a good idea to use a builtin... Change to own implementation when some sort of string.h is implemented. TODO!
                return addr;
            }
        } else {
            for(u64 *bmp = block->bitmap; (u64)bmp <= (u64)block->bitmap + block->bitmap_size; bmp++) {
                u64 map = ~(*bmp);
                if(map == 0) continue;        

                u64 mask = 0;
                u8 tmp_page_count = page_count;
                u8 bmp_idx = 0;
                u8 starting_map_idx = 63;

                do {
                    u64 shift_idx = 0;
                    asm volatile("bsr %1, %0" : "=r" (shift_idx) : "r" (map) : "rax", "rbx", "flags");

                    if(bmp_idx && shift_idx != 63) break; //We can only continue allocations from the MSB

                    if(shift_idx+1 >= tmp_page_count) {
                            mask = ((u64)1 << tmp_page_count) - 1;
                            mask <<= shift_idx+1-tmp_page_count;
                    } else {
                        if(!bmp_idx) mask = ((u64)1 << (shift_idx+1))-1;
                        else mask = ~(u64)0;
                    }

                    if((map & mask) == mask) {
                        if(bmp_idx == 0) starting_map_idx = shift_idx+1;
                        if(shift_idx+1 >= tmp_page_count) tmp_page_count = 0;
                        else {
                            tmp_page_count -= shift_idx+1;
                            bmp_idx++;
                            map = ~(*(bmp+bmp_idx));
                        }
                    } else {
                        //TODO: TEST WHEN BITMAP IS FRAGMENTED
                        //Skip MSB entries we just went over
                        map = (map ^ mask) & map;
                    }
                } while(tmp_page_count && map);

                if(tmp_page_count) continue;

                void* addr = (void*)((u64)block->addr + (((u64)bmp - (u64)block->bitmap) * 8 + (64-starting_map_idx)) * PMM_map_size);
                if ((u64)addr+size >= (u64)block->addr + block->size) break;

                if(page_count <= starting_map_idx) {
                    *bmp |= (((u64)1 << page_count)-1) << (starting_map_idx-page_count);
                    return addr;
                }

                *bmp |= ((u64)1 << (starting_map_idx)) - 1;
                page_count -= starting_map_idx;
                for(u64* bmp2 = bmp+1; bmp2 <= bmp+bmp_idx; bmp2++, page_count -= 64) {
                    u64 newbitmap = page_count >= 64 ? ~(u64)0 : (((u64)1 << page_count)-1) << (64-page_count);
                    *bmp2 |= newbitmap;
                }

                memset(addr, 0, size);
                return addr;
            }
        }
    }

    return NULL;
}

//TODO: HANDLE FREEING
void PMM_free(void *addr, u64 size) {
    size = align(size, PMM_map_size);

    struct PMM_memory_block_list_t *block_list = get_block_list();
    
    u32 page_count = size / PMM_map_size; //Number of PMM_map_size'd pages to allocate

    for(struct PMM_memory_block_t *block = block_list->addr; block; block = block->next) {
        if((u64)addr < (u64)MMU_make_addr_half(block->addr, MMU_addr_lower_half) || (u64)addr >= (u64)MMU_make_addr_half(block->addr, MMU_addr_lower_half) + block->size) continue;

        u64 offset = ((u64)addr - (u64)block->addr) / PMM_map_size;

        u64 bmp_idx = offset / 64;
        u64 starting_map_idx = 64-(offset % 64);

        u64 mask = 0, unmapped_pages = 0;
        if(page_count >= starting_map_idx) {
            if(starting_map_idx == 64) {
                mask = ~mask;
                unmapped_pages = 64;
            } else {
                mask = ((u64)1 << starting_map_idx) - 1;
                unmapped_pages = starting_map_idx;
            }
        } else {
            mask = (((u64)1 << page_count)-1) << (starting_map_idx-page_count);
            unmapped_pages = page_count;
        }

        do {
            u64* bmp = block->bitmap+bmp_idx;
            *bmp &= ~mask;

            bmp_idx++;
            page_count -= unmapped_pages;
            if(page_count >= 64) {
                mask = ~(u64)0;
                unmapped_pages = 64;
            } else {
                mask = (((u64)1 << page_count)-1) << (64-page_count);
                unmapped_pages = page_count;
            }
        } while (page_count);
    }

    return;
}

//TODO: HANDLE SMP BLOCK LIST BALOONING