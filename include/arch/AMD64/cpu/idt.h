#ifndef IDT_H
#define IDT_H

#include <types.h>

struct IDTR_t;
struct IDT_Gate_t;
struct IDT_t;
struct IDT_Table_t;

struct ISF_t { //Interrupt stack frame
    u64 r15, r14, r13, r12, r11, r10, r9, r8;
    u64 rbp, rdi, rsi, rdx, rcx, rbx, rax;
    u64 interrupt_number, error_code;
    u64 rip, cs, rflags, rsp, ss; //RSP is Return RSP. SS is null unless returning to compatibility mode is required.
} __attribute__((packed));

extern void X86_IDT_install(bool is_bootcore, u32 cpuId);

#endif