#ifndef KALLOC_H
#define KALLOC_H

#include <types.h>

void kalloc_init(void);
void* kalloc(u64 size);
void kfree(void* ptr);

#endif