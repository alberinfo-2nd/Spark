#ifndef GDT_H
#define GDT_H

#include <types.h>

struct GDTR_t;
struct GDT_Segment_t;
struct GDT_t;

extern void X86_GDT_install(void);

#endif