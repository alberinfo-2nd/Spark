#ifndef MMU_H
#define MMU_H

#include <types.h>

#define MMU_addr_lower_half     0
#define MMU_addr_higher_half    1
#define MMU_addr_kernel_half    2

#define MMU_PAGE_4K 0x1000
#define MMU_PAGE_2M 0x200000
#define MMU_PAGE_1G 0x40000000

#define MMU_FLAG_PRESENT 1
#define MMU_FLAG_RW 1 << 1
#define MMU_FLAG_SUPERVISOR 1 << 2
#define MMU_FLAG_PWT 1 << 3 //Write through
#define MMU_FLAG_PCD 1 << 4 //Cache disable
#define MMU_FLAG_PAT 1 << 5 //Page attribute table
#define MMU_FLAG_GLOBAL 1 << 6
#define MMU_FLAG_NX 1 << 7

void MMU_init(void* PML4);
void* MMU_get_cr3(void);
void MMU_switch_cr3(void* PML4); //Switches the current address space in the respective cpu
u8 MMU_map_page(void* address_space, void* paddr, void* vaddr, u32 page_size, u32 flags);
u8 MMU_map_range(void* address_space, void* paddr, void* vaddr, u64 size, u32 page_size, u32 flags);
void MMU_unmap(void* address_space, void* vaddr);
void* MMU_get_paddr(void* address_space, void* vaddr);
void MMU_invlpg(void* vaddr); //INVLPG will always execute on the current address space
bool MMU_is_canonical(void *addr);
int MMU_get_address_half(void *addr); //Returns which section of the memory the address lives in
void* MMU_make_addr_half(void *addr, int half); //Returns the corresponding address if it lived in X half (i.e, a lower-half address equivalent to its higher half address)

#endif