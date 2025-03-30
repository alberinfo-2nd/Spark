#ifndef GDT_H
#define GDT_H

#include <types.h>

struct GDTR_t;
struct GDT_Segment_t;
struct GDT_t;
struct GDT_Table_t;

extern void X86_GDT_install(bool is_bootcore, u32 cpuId);

#endif