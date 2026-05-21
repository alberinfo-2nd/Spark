#ifndef EALLOC_H
#define EALLOC_H

#include <types.h>

void* ekalloc(u32 size);
void ekalloc_finish(void);

#endif