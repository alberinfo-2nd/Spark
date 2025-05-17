#include <kernel/mm/pmm.h>
#include <kernel/sync/spinlock.h>

struct PMM_memory_block_t {
    u64 *ptr;
    u64 size; //Size in bytes
    struct PMM_memory_block_t *next;
    struct spinlock_t lock;
};

//static u8 initial_memory[4096+sizeof(struct PMM_memory_block_t)] __attribute__((section(".data")));

struct PMM_memory_block_t PMM_start;

void PMM_init() {
    
}

void* PMM_alloc_aligned(u64 size, u64 alignment) {

}