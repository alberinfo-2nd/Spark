#ifndef PORTS_H
#define PORTS_H

#include <types.h>

static inline void outportb(u16 port, u8 data) {
    asm volatile("outb %b0, %w1" : : "a" (data), "d" (port) : "memory");
}

static inline u8 inportb(u16 port) {
    u8 retval = 0;
    asm volatile("inb %w1, %b0" : "=a" (retval) : "d" (port) : "memory");
    return retval;
}

static inline void outportw(u16 port, u16 data) {
    asm volatile("outw %w0, %w1" : : "a" (data), "d" (port) : "memory");
}

static inline u16 inportw(u16 port) {
    u16 retval = 0;
    asm volatile("inw %w1, %w0" : "=a" (retval) : "d" (port) : "memory");
    return retval;
}

static inline void outportl(u16 port, u32 data) {
    asm volatile("outl %l0, %w1" : : "a" (data), "d" (port) : "memory");
}

static inline u32 inportd(u16 port) {
    u32 retval = 0;
    asm volatile("inl %w1, %l0" : "=a" (retval) : "d" (port) : "memory");
    return retval;
}

static inline void port_io_wait(void) {
    outportb(0x80, 0); //Recommended by the Osdev Wiki as a mini-wait needed for older PCs
}

#endif