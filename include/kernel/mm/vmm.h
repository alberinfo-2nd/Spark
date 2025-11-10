#ifndef VMM_H
#define VMM_H

#include <types.h>

#define VMM_TYPE_DYNAMIC 0
#define VMM_TYPE_BACKED 1
#define VMM_TYPE_MMIO 2
#define VMM_TYPE_SWAP 3
#define VMM_TYPE_RESERVED 4

//Each process has its own address space. This includes having different address spaces between different cores. TODO: How to handle?
struct VMM_Address_Space_t {
    //Normally points to PML4. Maybe PML5 in the future?
    void* CR3;

    //Tree ordered by address - Used to quickly find available spaces that have to meet constraints (i.e. <4GiB) or when freeing.
    struct AVL_tree_t *address_tree;
    
    //Tree ordered by size - Used to quickly allocate memory.
    struct AVL_tree_t *size_tree;

    //List of all address ranges ordered by address. Both free and allocated ranges go here
    struct Address_space_range_t *range_list;
};

void VMM_init(void);
void VMM_add_free_range(struct VMM_Address_Space_t* address_space, u64 address, u64 size); //Add a free range to the provided address space
struct VMM_Address_Space_t* VMM_create_address_space(void);
void VMM_switch_address_space(struct VMM_Address_Space_t* newAddressSpace);
void* VMM_alloc(struct VMM_Address_Space_t* address_space, u64 size, u8 type, u32 flags, u32 page_size);
int VMM_free(struct VMM_Address_Space_t* address_space, void* ptr);
void VMM_page_fault_handler(void* _regs);

#endif