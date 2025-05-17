#ifndef PMM_H
#define PMM_H

#include <types.h>

void PMM_init();
void* PMM_alloc_aligned(u64 size, u64 alignment);

#endif