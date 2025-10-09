#include <kernel/mm/kalloc.h>
#include <kernel/mm/ekalloc.h>

//TODO: Potentially make kalloc be usable across mutliple cores at the same time?

// struct ALLOC_memory_block_t {
//     u64 *bitmap;
//     u64 bitmap_size;
//     u64 *addr;
    
// };


bool kalloc_initialized = false;

void kalloc_init(void) {
    ekalloc_finish();
    kalloc_initialized = true;
}

void* kalloc(u64 size) {
    if(!kalloc_initialized) return ekalloc(size);

    //Actually allocate some memory properly
    return NULL;
}

void kfree(void *ptr) {
    if(!kalloc_initialized) return;
}