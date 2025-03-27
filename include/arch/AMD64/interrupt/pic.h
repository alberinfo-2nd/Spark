#ifndef PIC_H
#define PIC_H

#include <types.h>

void X86_PIC_remap(u8 master_offset, u8 slave_offset);
void X86_PIC_enable(void);
void X86_PIC_disable(void);
void X86_PIC_send_eoi(u8 IRQn);
void X86_PIC_mask(u8 IRQn);
void X86_PIC_unmask(u8 IRQn);

#endif