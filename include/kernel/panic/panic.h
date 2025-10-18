#ifndef PANIC_H
#define PANIC_H

#include <types.h>
#include <arch/AMD64/cpu/idt.h>

void kpanic(const char* message, struct ISF_t *regs);

#endif