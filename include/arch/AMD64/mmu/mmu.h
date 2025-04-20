#ifndef MMU_H
#define MMU_H

#include <types.h>

void MMU_switch_cr3(void* PML4, u32 cpuId); //Switches the current address space in the respective cpu
void MMU_map(void* PML4, void* paddr, void* vaddr, u32 flags, u16 PCID);
void MMU_unmap(void* PML4, void* vaddr);
void MMU_get_paddr(void* PML4, void* vaddr);
void MMU_invlpg(void* vaddr); //INVLPG will always execute on the current address space

#endif