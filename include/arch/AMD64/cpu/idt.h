#ifndef IDT_H
#define IDT_H

#include <types.h>

struct IDTR_t;
struct IDT_Gate_t;
struct IDT_t;
struct IDT_Table_t;

extern void install_idt(bool is_bootcore, u32 cpuId);

#endif