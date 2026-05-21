#include <kernel/mm/pmm.h>
#include <kernel/sync/spinlock.h>
#include <arch/AMD64/cpu/cpu.h>
#include <arch/AMD64/mmu/mmu.h>
#include <arch/AMD64/cpu/lapic.h>
#include <kernel/debug/log.h>

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

// Helper function that rotates the mask which contains a pattern that repeats every page_alignment_interval bits.
// Examples (in 8 bits, as its easier to visualize that way):
//  For page_alignment_interval = 2, mask_shift = 0, masks: 10101010 10101010 [...]
//  For page_alignment_interval = 3, mask_shift = 1, masks: 10010010 01001001 00100100 10010010
//  For page_alignment_interval = 5, mask_shift = 2, masks: 10000100 00100001 00000100 01000010
//  For page_alignment_interval = 6, mask_shift = 4, masks: 10000010 00001000 00100000 10000010

static inline u64 shift_mask(u64 mask, u32 mask_shift, u32 page_alignment_interval) {
    //We have to shift by however many bitmap entries we skipped
    for(u32 i = 0; i <= page_alignment_interval / 64; i++) {
        u64 first_mask = 0;
        asm volatile("bsr %1, %0" : "=r" (first_mask) : "r" (mask) : "rax", "rbx", "flags"); //Search for the first set bit in the mask
        
        first_mask -= mask_shift; //Calculate the position of the first bit in the mask with respect to the mask after its shifted

        if(first_mask + page_alignment_interval >= 64) mask >>= mask_shift;
        else mask = (mask >> mask_shift) | (1ULL << (first_mask + page_alignment_interval)); //Shift the mask and set the (possibly) new MSB in the mask
    }
    return mask;
}

//TODO: Possibly handle more things such as placement (for things like DMA, MMIOs and the like)
void* PMM_alloc_aligned(u64 size, u64 alignment) {
    if(alignment == 0) alignment = PMM_map_size;

    size = align(size, PMM_map_size);
    alignment = align(alignment, PMM_map_size);

    struct PMM_memory_block_list_t *block_list = get_block_list();
    
    u32 page_count = size / PMM_map_size; //Number of PMM_map_size'd pages to allocate
    u32 page_alignment_interval = alignment / PMM_map_size; //Every how many pages the alignment happens
    u32 mask_shift = (page_alignment_interval - (64 % page_alignment_interval)) % page_alignment_interval; //How many bits the mask shifts for every bitmap entry traveled

    for(struct PMM_memory_block_t *block = block_list->addr; block; block = block->next) {
        u64 first_aligned_address = align((u64)block->addr, alignment); //First address that is aligned to the requested amount
        u32 alignment_shift = (first_aligned_address - (u64)block->addr) / PMM_map_size; // How many maps we need to skip in order to get the first aligned address

        u64 alignment_mask = 0; //Mark where map addresses that would satisfy the requested alignment are
        if(page_alignment_interval == 1) alignment_mask = ~0ULL;
        else {
            for(int i = 64 - alignment_shift % 64 - 1; i >= 0; i -= page_alignment_interval) {
                alignment_mask |= 1ULL << i;
            }
        }

        //For some reason doing the pointer arithmetic in the initialization of the loops below sets bmp to zero instead of its correct value....? Recheck
        u64* bmp = block->bitmap + alignment_shift/64;

        if(page_count == 1) {
            for(; (u64)bmp <= (u64)block->bitmap + block->bitmap_size; bmp+=align(page_alignment_interval, 64)/64, alignment_mask = shift_mask(alignment_mask, mask_shift, page_alignment_interval)) {                
                u64 map = ~(*bmp) & alignment_mask;
                if(map == 0) continue;

                u64 map_idx = 0;

                asm volatile("bsr %1, %0" : "=r" (map_idx) : "r" (map) : "rax", "rbx", "flags");

                void* addr = (void*)((u64)block->addr + (((u64)bmp - (u64)block->bitmap) * 8 + (64-map_idx-1)) * PMM_map_size);
                if ((u64)addr+size >= (u64)block->addr + block->size) break; //If the end of the allocation goes over the end of the block, then ignore it and try again.
                *bmp |= 1ULL << map_idx;

                return addr;
            }
        } else {
            for(; (u64)bmp <= (u64)block->bitmap + block->bitmap_size; bmp+=align(page_alignment_interval, 64)/64, alignment_mask = shift_mask(alignment_mask, mask_shift, page_alignment_interval)) {
                u64 map = ~(*bmp);
                if((map & alignment_mask) == 0) continue;        

                u64 mask = 0;
                u32 tmp_page_count = page_count;
                u8 bmp_idx = 0;
                u8 starting_map_idx = 63;

                u64 alignment_mask_copy = alignment_mask;

                do {
                    if((map & alignment_mask_copy) == 0) break; //bsr on an empty register is undefined.

                    u64 shift_idx = 0;
                    asm volatile("bsr %1, %0" : "=r" (shift_idx) : "r" (map & alignment_mask_copy) : "cc");

                    if(bmp_idx && shift_idx != 63) break; //We can only continue allocations from the MSB

                    if(shift_idx+1 >= tmp_page_count) {
                        if(tmp_page_count < 64) mask = (1ULL << tmp_page_count) - 1;
                        else mask = ~0ULL;
                        mask <<= shift_idx+1-tmp_page_count;
                    } else {
                        if(!bmp_idx) mask = (1ULL << (shift_idx+1))-1;
                        else mask = ~0ULL;
                    }

                    if((map & mask) == mask) {
                        if(bmp_idx == 0) starting_map_idx = shift_idx+1;
                        if(shift_idx+1 >= tmp_page_count) tmp_page_count = 0;
                        else {
                            tmp_page_count -= shift_idx+1;
                            bmp_idx++;
                            map = ~(*(bmp+bmp_idx));
                        }

                        alignment_mask_copy = ~0ULL;
                    } else {
                        map &= ~mask; //Skip MSB entries that do not align with the mask.
                    }
                } while(tmp_page_count && map);

                if(tmp_page_count) continue;

                void* addr = (void*)((u64)block->addr + (((u64)bmp - (u64)block->bitmap) * 8 + (64-starting_map_idx)) * PMM_map_size);
                if ((u64)addr+size >= (u64)block->addr + block->size) break;

                if(page_count <= starting_map_idx) {
                    *bmp |= ((1ULL << page_count)-1) << (starting_map_idx-page_count);
                    return addr;
                }

                *bmp |= (1ULL << (starting_map_idx+1)) - 1;
                page_count -= starting_map_idx;
                for(u64* bmp2 = bmp+1; bmp2 <= bmp+bmp_idx; bmp2++, page_count -= 64) {
                    u64 newbitmap = page_count >= 64 ? ~0ULL : ((1ULL << page_count)-1) << (64-page_count);
                    *bmp2 |= newbitmap;
                }

                return addr;
            }
        }
    }

    return NULL;
}

//TODO: Test
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
                mask = (1ULL << starting_map_idx) - 1;
                unmapped_pages = starting_map_idx;
            }
        } else {
            mask = ((1ULL << page_count)-1) << (starting_map_idx-page_count);
            unmapped_pages = page_count;
        }

        do {
            u64* bmp = block->bitmap+bmp_idx;
            *bmp &= ~mask;

            bmp_idx++;
            page_count -= unmapped_pages;
            if(page_count >= 64) {
                mask = ~0ULL;
                unmapped_pages = 64;
            } else {
                mask = ((1ULL << page_count)-1) << (64-page_count);
                unmapped_pages = page_count;
            }
        } while (page_count);
    }

    return;
}

//TODO: HANDLE SMP BLOCK LIST BALOONING