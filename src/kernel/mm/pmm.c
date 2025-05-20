#include <kernel/mm/pmm.h>
#include <kernel/sync/spinlock.h>

struct PMM_memory_block_t {
    u64 *bitmap;
    u64 *addr;
    u64 size; //Size in bytes excluding the bitmap
    struct PMM_memory_block_t *next;
    struct spinlock_t lock;
};

struct PMM_memory_block_t *PMM_start = (struct PMM_memory_block_t*)0;

void PMM_add_block(void* addr, u64 size) {
    struct PMM_memory_block_t *block = (struct PMM_memory_block_t*)addr;
    block->next = PMM_start;
    PMM_start = block;

    u64 bitmap_size = (size-sizeof(struct PMM_memory_block_t)) / 4096 / 8; //How many bytes are needed for the bitmap

    block->bitmap = (u64*)((u64)addr + sizeof(struct PMM_memory_block_t));
    block->addr = (u64*)((u64)block->bitmap + bitmap_size);

    block->next = NULL;
    block->lock.state = false;
}

void* PMM_alloc_aligned(u64 size, u64 alignment) {

}