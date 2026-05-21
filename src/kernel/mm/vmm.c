#include <kernel/mm/vmm.h>
#include <kernel/mm/pmm.h>
#include <arch/AMD64/mmu/mmu.h>
#include <kernel/mm/kalloc.h>
#include <kernel/debug/log.h>
#include <arch/AMD64/cpu/cpu.h>
#include <arch/AMD64/cpu/idt.h>

#define align(x, y) ((u64)x & ~((u64)y-1))

#define max(a, b) (a > b ? a : b)
#define nodeHeight(node) (node ? node->height : -1)
#define balance(node) (nodeHeight(node->child[AVL_CHILD_LEFT]) - nodeHeight(node->child[AVL_CHILD_RIGHT]))
#define leftLeaning(node) (balance(node) > 1)
#define rightLeaning(node) (balance(node) < -(i8)1)

#define VMM_STATUS_RAM_FREE         0 //RAM memory - Free to be used
#define VMM_STATUS_RAM_ALLOCATED    1 //RAM memory - Backed by a page
#define VMM_STATUS_RAM_SWAP         2 //RAM memory - Sent to swap
#define VMM_STATUS_FILE_PRESENT     3 //Memory mapped file - currently in memory
#define VMM_STATUS_FILE_NOTPRESENT  4 //Memory mapped file - not loaded in memory
#define VMM_STATUS_UNUSABLE         5 //Unusable memory - MMIO, DMA, etc. Could also be RAM that is backed by a page, therefore becoming unusable

//Defines a link between 1 Virtual range and n physical pages.
//TODO: Possibly change to some type of tree afterwards?
struct Address_space_page_t {
    struct Address_space_page_t* next;
    u64 vaddr;
    u64 paddr;
    u64 size;
};

struct Address_space_range_t {
    struct Address_space_range_t* next; //Next range, sorted by address. includes both free and allocated ranges
    struct Address_space_page_t* physical_pages; //All of the physical pages backing this virtual memory range
    struct Address_space_page_t* last_physical_page;

    u64 address;
    u64 size;

    //Caching information?
    //Permissions?
    u32 flags; //Same as MMU_FLAGS_*
    u32 page_size;

    u8 type;
    u8 status;

    //File?
};

struct AVL_node_t {
    struct AVL_node_t* parent;
#define AVL_CHILD_LEFT  0
#define AVL_CHILD_RIGHT 1
    struct AVL_node_t* child[2];
    struct Address_space_range_t* range;
    u64 key;
    i8 height;
};

//Use this to set the correct reference in the Address space range when inserting a new node
#define AVL_TREE_ADDRESS 0
#define AVL_TREE_SIZE 1

struct AVL_tree_t {
    struct AVL_node_t* root;
};

/* ------------------------------------------------------------------------------------------------------------- */

//Finds node with provided key. Can return null if tree is empty or key is not present.
struct AVL_node_t* AVL_search(struct AVL_node_t* node, u64 key) {
    while (node) {
        if(node->key == key) return node;
        node = node->child[AVL_CHILD_RIGHT - (int)(node->key > key)];
    }

    return NULL;
}

//Finds node whose key is closest to the one provided (but still bigger). Can return null if there is no match or if tree is empty
struct AVL_node_t* AVL_search_closest_upper(struct AVL_node_t* node, u64 key) {
    if(node == NULL) return NULL;
    struct AVL_node_t* best_match = node;

    while(node) {
        if(node->key <= best_match->key && node->key >= key) best_match = node;

        if(node->key == key) return node;
        node = node->child[AVL_CHILD_RIGHT - (int)(node->key > key)];
    }

    return best_match;
}

//Finds node whose key is closest to the one provided (but still smaller). Can return null if there is no match or if tree is empty
struct AVL_node_t* AVL_search_closest_lower(struct AVL_node_t* node, u64 key) {
    if(node == NULL) return NULL;
    struct AVL_node_t* best_match = node;

    while(node) {
        if(node->key >= best_match->key && node->key <= key) best_match = node;

        if(node->key == key) return node;
        node = node->child[AVL_CHILD_RIGHT - (int)(node->key > key)];
    }

    return best_match;
}

struct AVL_node_t* AVL_get_minimum(struct AVL_node_t* node) {
    while(node->child[AVL_CHILD_LEFT]) node = node->child[AVL_CHILD_LEFT];
    return node;
}

struct AVL_node_t* AVL_get_maximum(struct AVL_node_t* node) {
    while(node->child[AVL_CHILD_RIGHT]) node = node->child[AVL_CHILD_RIGHT];
    return node;
}

struct AVL_node_t* AVL_get_successor(struct AVL_node_t* node) {
    if(node->child[AVL_CHILD_RIGHT]) return AVL_get_minimum(node->child[AVL_CHILD_RIGHT]);
    while(node->parent && node->parent->child[AVL_CHILD_RIGHT] == node) node = node->parent; //This can be null if the current node is the rightmost node (No successor)

    return node;
}

struct AVL_node_t* AVL_get_predecessor(struct AVL_node_t* node) {
    if(node->child[AVL_CHILD_LEFT]) return AVL_get_maximum(node->child[AVL_CHILD_LEFT]);
    while(node->parent && node->parent->child[AVL_CHILD_LEFT] == node) node = node->parent;
    return node;
}

struct AVL_node_t* AVL_left_rotate(struct AVL_node_t* node) {
    struct AVL_node_t* right_child = node->child[AVL_CHILD_RIGHT]; //We know with certainty that since node is right leaning, there is at least one node on the right.
    struct AVL_node_t* middle_subtree = right_child->child[AVL_CHILD_LEFT]; //May be null

    right_child->parent = node->parent;
    if(middle_subtree != NULL) middle_subtree->parent = node;
    node->parent = right_child;

    node->child[AVL_CHILD_RIGHT] = middle_subtree;
    right_child->child[AVL_CHILD_LEFT] = node;

    node->height = max(nodeHeight(node->child[AVL_CHILD_LEFT]), nodeHeight(node->child[AVL_CHILD_RIGHT])) + 1;
    right_child->height = max(nodeHeight(right_child->child[AVL_CHILD_LEFT]), nodeHeight(right_child->child[AVL_CHILD_RIGHT])) + 1;

    if(right_child->parent != NULL) right_child->parent->child[AVL_CHILD_RIGHT - (int)(right_child->parent->key > right_child->key)] = right_child;

    return right_child;
}

struct AVL_node_t* AVL_right_rotate(struct AVL_node_t* node) {
    struct AVL_node_t* left_child = node->child[AVL_CHILD_LEFT]; //We know with certainty that since node is left leaning, there is at least one node on the left.
    struct AVL_node_t* middle_subtree = left_child->child[AVL_CHILD_RIGHT]; //May be null

    left_child->parent = node->parent;
    if(middle_subtree != NULL) middle_subtree->parent = node;
    node->parent = left_child;

    node->child[AVL_CHILD_LEFT] = middle_subtree;
    left_child->child[AVL_CHILD_RIGHT] = node;

    node->height = max(nodeHeight(node->child[AVL_CHILD_LEFT]), nodeHeight(node->child[AVL_CHILD_RIGHT])) + 1;
    left_child->height = max(nodeHeight(left_child->child[AVL_CHILD_LEFT]), nodeHeight(left_child->child[AVL_CHILD_RIGHT])) + 1;

    if(left_child->parent != NULL) left_child->parent->child[AVL_CHILD_RIGHT - (int)(left_child->parent->key > left_child->key)] = left_child;

    return left_child;
}

struct AVL_node_t* AVL_balance(struct AVL_node_t* node) {
    node->height = max(nodeHeight(node->child[AVL_CHILD_LEFT]), nodeHeight(node->child[AVL_CHILD_RIGHT])) + 1;

    if(rightLeaning(node)) {
        if(balance(node->child[AVL_CHILD_RIGHT]) >= 1) node->child[AVL_CHILD_RIGHT] = AVL_right_rotate(node->child[AVL_CHILD_RIGHT]);
        return AVL_left_rotate(node);
    }

    if(leftLeaning(node)) {
        if(balance(node->child[AVL_CHILD_LEFT]) <= -(i8)1) node->child[AVL_CHILD_LEFT] = AVL_left_rotate(node->child[AVL_CHILD_LEFT]);
        return AVL_right_rotate(node);
    }

    return node;
}

struct AVL_node_t* AVL_insert(struct AVL_tree_t* tree, struct Address_space_range_t* range, u64 key) {
    struct AVL_node_t* prevNode = NULL;
    struct AVL_node_t* node = tree->root;
    while(node != NULL) {
        prevNode = node;
        node = node->child[AVL_CHILD_RIGHT - (int)(node->key > key)];
    }

    struct AVL_node_t* newNode = kalloc(sizeof(struct AVL_node_t));
    newNode->child[AVL_CHILD_LEFT] = NULL;
    newNode->child[AVL_CHILD_RIGHT] = NULL;
    newNode->height = 0;
    newNode->range = range;
    newNode->key = key;
    newNode->parent = prevNode;

    if(prevNode == NULL) {
        tree->root = newNode;
        return newNode;
    }

    prevNode->child[AVL_CHILD_RIGHT - (int)(prevNode->key > key)] = newNode;

    do {
        prevNode = AVL_balance(prevNode);
        if(prevNode->parent == NULL) {
            tree->root = prevNode;
            break;
        }

        prevNode = prevNode->parent;
    } while(prevNode);

    return newNode;
}

//TODO: TEST PROPERLY
void AVL_delete(struct AVL_tree_t* tree, struct AVL_node_t* node) {
    if(node->child[AVL_CHILD_LEFT] == NULL || node->child[AVL_CHILD_RIGHT] == NULL) {
        if(node->parent == NULL) {
            tree->root = node->child[AVL_CHILD_RIGHT] ? node->child[AVL_CHILD_RIGHT] : node->child[AVL_CHILD_LEFT];
            tree->root->parent = NULL;
        } else {
            node->parent->child[AVL_CHILD_RIGHT - (int)(node->parent->key > node->key)] = node->child[AVL_CHILD_RIGHT] ? node->child[AVL_CHILD_RIGHT] : node->child[AVL_CHILD_LEFT];
            node->parent->child[AVL_CHILD_RIGHT - (int)(node->parent->key > node->key)]->parent = node->parent;
        }
    } else {
        struct AVL_node_t* succesor = AVL_get_successor(node);
        node->key = succesor->key;
        node->range = succesor->range;
        AVL_delete(tree, succesor);
        return;
    }

    struct AVL_node_t* nextNode = node->parent;
    kfree(node);

    if(nextNode == NULL) nextNode = tree->root;
    do {
        AVL_balance(nextNode);
        nextNode = nextNode->parent;
    } while(nextNode);

    return;
}

//TODO: join, split, union?

void AVL_trasverse_inorder(struct AVL_node_t* node) {
    if(node == NULL) return;

    AVL_trasverse_inorder(node->child[AVL_CHILD_LEFT]);
    DEBUG_log((const string)"AVL Node with key %x ", node->key);
    AVL_trasverse_inorder(node->child[AVL_CHILD_RIGHT]);
}

/* ------------------------------------------------------------------------------------------------------------- */

void VMM_add_free_range(struct VMM_Address_Space_t* address_space, u64 address, u64 size) {
    struct Address_space_range_t* range = kalloc(sizeof(struct Address_space_range_t));
    range->next = NULL;
    range->address = address;
    range->size = size;
    range->page_size = 0;

    range->type = VMM_TYPE_RAM;
    range->status = VMM_STATUS_RAM_FREE;

    range->physical_pages = NULL;
    range->last_physical_page = NULL;
    

    AVL_insert(address_space->size_tree, range, size);
    struct AVL_node_t* previousNode = AVL_search_closest_lower(address_space->address_tree->root, address);
    if(previousNode == NULL) {
        range->next = address_space->range_list;
        address_space->range_list = range;
        return;
    }

    struct Address_space_range_t* previousRange = previousNode->range;
    while(previousRange->next && previousRange->next->address < address) previousRange = previousRange->next;
    range->next = previousRange->next;
    previousRange->next = range;
}

void VMM_range_push_physical_page(struct Address_space_range_t* range, u64 paddr, u64 vaddr, u64 size) {
    struct Address_space_page_t* newPage = kalloc(sizeof(struct Address_space_page_t));
    newPage->next = NULL;
    newPage->paddr = paddr;
    newPage->vaddr = vaddr;
    newPage->size = size;

    if(range->physical_pages == NULL) range->physical_pages = range->last_physical_page = newPage;
    range->last_physical_page->next = newPage;
    range->last_physical_page = range->last_physical_page->next;
}

void VMM_range_pop_physical_pages(struct VMM_Address_Space_t* address_space, struct Address_space_range_t* range) {
    if(address_space != NULL) MMU_unmap_range(address_space->CR3, (void*)range->address, range->size);

    for(struct Address_space_page_t* currPage = range->physical_pages; currPage != NULL; currPage = currPage->next) {
        PMM_free((void*)currPage->paddr, currPage->size);
        kfree(currPage);
    }

    range->physical_pages = NULL;
}

void VMM_init(void) {
    struct VMM_Address_Space_t* address_space = kalloc(sizeof(struct VMM_Address_Space_t));
    address_space->CR3 = MMU_get_cr3();
    address_space->address_tree = kalloc(sizeof(struct AVL_tree_t));
    address_space->address_tree->root = NULL;
    address_space->size_tree = kalloc(sizeof(struct AVL_tree_t));
    address_space->size_tree->root = NULL;
    address_space->range_list = NULL;

    //TODO: Make it also add the currently allocated areas....??? its not really necessary but would be a good addition
    //Populates the address space
    MMU_travel_address_space(address_space);

    // kalloc_init();

    X86_CPU_get_self()->address_space = address_space;
    return;
}

struct VMM_Address_Space_t* VMM_create_address_space(void) {
    struct VMM_Address_Space_t* address_space = kalloc(sizeof(struct VMM_Address_Space_t));
    address_space->CR3 = PMM_alloc_aligned(MMU_PAGE_4K, 4096); //Reserve a 4KiB Page for the new PML4, that is 4K aligned
    address_space->address_tree = kalloc(sizeof(struct AVL_tree_t));
    address_space->address_tree->root = NULL;
    address_space->size_tree = kalloc(sizeof(struct AVL_tree_t));
    address_space->size_tree->root = NULL;
    address_space->range_list = NULL;

    //TODO: COPY CR3!!!!!
    //Populates the address space
    MMU_travel_address_space(address_space);
    return address_space;
}

struct VMM_Address_Space_t* VMM_get_current_address_space(void) {
    return X86_CPU_get_self()->address_space;
}

void VMM_destroy_address_space(struct VMM_Address_Space_t* address_space) {
    //Providing the address space is unneded, as all mappings will be inmediately undone the moment CR3 is freed from memory.
    //TODO: Check if memory allocated for tables that are not CR3 are actually free'd
    for(struct Address_space_range_t* currRange = address_space->range_list; currRange != NULL; currRange = currRange->next) {
        VMM_range_pop_physical_pages(address_space, currRange);
    }

    //TODO: Liberate other resources such as files, MMIOs and so on.
}

//Allocates n bytes within the provided address space. Each memory type has some default options enforced
//(Such as being read-only, MMIO being uncacheable, etc), which may be overridden by the flags parameter (uses MMU_FLAG_*)
//page_size uses MMU_PAGE_X as size
void* VMM_alloc(struct VMM_Address_Space_t* address_space, u64 size, u8 type, u32 flags, u32 page_size) {
    if((size & ~(MMU_PAGE_4K-1)) != size) return NULL; //We dont deal with sizes that are not aligned to at least a 4KiB page

    if(type > VMM_TYPE_RESERVED) return NULL; //Unknown VMM_TYPE

    if(address_space == NULL) address_space = VMM_get_current_address_space(); //Maybe also check if the address space is valid / real?

    struct AVL_node_t* node = AVL_search_closest_upper(address_space->size_tree->root, size);
    if(node == NULL) return NULL; //NO SPACE!!!!

    struct Address_space_range_t* range = node->range;
    void* ptr = (void*)range->address;

    range->size -= size;
    range->address += size;

    //Delete-reinsert, but TODO check if that is needed 100% of the time
    AVL_delete(address_space->size_tree, node);
    AVL_insert(address_space->size_tree, range, range->size);

    //TODO: Check if flags are correct
    struct Address_space_range_t* newRange = kalloc(sizeof(struct Address_space_range_t));
    newRange->next = NULL;
    newRange->address = (u64)ptr;
    newRange->size = size;
    newRange->page_size = page_size;

    newRange->type = type;
    newRange->status = VMM_STATUS_RAM_FREE;
    newRange->flags = flags;

    newRange->physical_pages = NULL;
    newRange->last_physical_page = NULL;

    struct AVL_node_t* newNode = AVL_insert(address_space->address_tree, newRange, newRange->address);
    struct AVL_node_t* previousNode = AVL_get_predecessor(newNode);
    if(previousNode == NULL) {
        range->next = address_space->range_list;
        address_space->range_list = range;
    } else {
        struct Address_space_range_t* previousRange = previousNode->range;
        while(previousRange->next && previousRange->next->address < range->address) previousRange = previousRange->next;
        range->next = previousRange->next;
        previousRange->next = range;
    }

    switch(type) {
        case VMM_TYPE_RAM:
            newRange->status = VMM_STATUS_RAM_FREE;
            break;
        case VMM_TYPE_RAM_BACKED:
            newRange->status = VMM_STATUS_RAM_ALLOCATED;

            void* phys = PMM_alloc_aligned(size, page_size);
            if(phys == NULL) {} //Out of memory, I guess. What do we do?
            int res = MMU_map_range(NULL, phys, ptr, size, page_size, flags | MMU_FLAG_PRESENT, 0);
            if(res != 0) {} //There was an error mapping. What do we do?

            VMM_range_push_physical_page(newRange, (u64)phys, (u64)ptr, size);
            break;
        case VMM_TYPE_MMIO:
            newRange->status = VMM_STATUS_UNUSABLE;
            break;
        case VMM_TYPE_FILE:
            //files are also AOW, therefore status switches to VMM_STATUS_FILE_PRESENT once the page fault rises
            newRange->status = VMM_STATUS_FILE_NOTPRESENT;
            break;
        case VMM_TYPE_RESERVED:
            newRange->status = VMM_STATUS_UNUSABLE;
            break;
        default:
            //Either does not go here or its not implemented yet
            //Kill process, or panic!
            break;
    }

    return ptr;
}


//
// 
// Returns 0 when succesfully freed
// Returns 1 when a parameter is wrong
// 
// 

int VMM_free(struct VMM_Address_Space_t *address_space, void *ptr) {
    if(address_space == NULL) address_space = VMM_get_current_address_space();

    struct AVL_node_t* node = AVL_search(address_space->address_tree->root, (u64)ptr);
    if(node == NULL) return 1; //If the node does not exist in the address_tree, then the address was never allocated!!!

    struct Address_space_range_t* range = node->range;
    switch (range->type) {
        case VMM_TYPE_RAM:
        case VMM_TYPE_RAM_BACKED:
            switch(range->status) {
                case VMM_STATUS_RAM_FREE: //The ram was never used, therefore there is nothing else we need to do. range->type cannot be TYPE_RAM_BACKED!
                    break;

                case VMM_STATUS_RAM_ALLOCATED: //This means it was AOW and it got allocated. Free the memory behind this page
                    VMM_range_pop_physical_pages(address_space, range);
                    break;

                case VMM_STATUS_RAM_SWAP: //Memory was sent to swap
                    break;

                default:
                    //Range is corrputed?
            }
            break;

        case VMM_TYPE_MMIO:
            break;

        case VMM_TYPE_FILE:
            break;

        case VMM_TYPE_RESERVED:
            break;

        default:
            //Range is corrupted?
    }

    range->flags = 0;
    range->type = VMM_TYPE_RAM;
    range->status = VMM_STATUS_RAM_FREE;
    range->page_size = 0;

    struct AVL_node_t* predecessor = AVL_get_predecessor(node); //Previous allocated address; We can use this information to coalesce this range we are freeing with the rest
    struct AVL_node_t* successor = AVL_get_successor(node); //Same as above but for next allocated address

    AVL_delete(address_space->address_tree, node);

    if(predecessor) {
        //This is the allocated range right before the one that is being freed. There will be at most one free range between each allocated one.
        struct Address_space_range_t* previousRange = predecessor->range;
        if(previousRange->next && previousRange->next->address < range->address) {
            previousRange->next->size += range->size;
            previousRange->next->next = range->next;
            kfree(range);
            range = previousRange->next;
        }
    }

    if(successor) {
        struct Address_space_range_t* nextRange = successor->range;
        if(range->next && range->next->address < nextRange->address) {
            range->size += range->next->size;
            
            struct Address_space_range_t* rangeToLink = range->next->next;
            kfree(range->next);
            range->next = rangeToLink;
        }
    }

    AVL_insert(address_space->size_tree, range, range->size);

    //Unmapping is handled by VMM_range_pop_physical_pages
    return 0;
}

#define VMM_PF_ERRCODE_PRESENT  1 << 0  //Set if PF was caused by a protection violation, non-present page otherwise
#define VMM_PF_ERRCODE_RW       1 << 1  //Set if PF was caused by a write access, read otherwise
#define VMM_PF_ERRCODE_US       1 << 2  //Set if PF was caused in user mode, supervisor mode otherwise
#define VMM_PF_ERRCODE_RSV      1 << 3  //Set if PF was caused by a 1 in a reserved field within the page-translation entry
#define VMM_PF_ERRCODE_ID       1 << 4  //Set if PF was caused instruction fetch (only when NX-bit is enabled in EFER && PAE is set), data access otherwise
#define VMM_PF_ERRCODE_PK       1 << 5  //set if PF was caused by a protection key violation in a user-mode address (only when CR4.PKE is set)
#define VMM_PF_ERRCODE_SS       1 << 6  //Set if PF was caused by a shadow stack access (only when CR4.CET is set)
#define VMM_PF_ERRCODE_RMP      1 << 31 //Set if PF was caused by a Reverse Map Table Violation (for virtualization, only when SYSCFG[SecureNestedPagingEn] is set)

void VMM_page_fault_handler(void* _regs) {
    struct ISF_t* regs = (struct ISF_t*)_regs;
    u64 fault_address = 0;
    asm volatile("mov %%cr2, %0 " : "=r" (fault_address));

    //Get current VMM_address_space
    struct VMM_Address_Space_t* address_space = X86_CPU_get_self()->address_space;

    //Was the PF caused by a reserved field being set?
        //YES - Fix it!
        //NO  - Nothing to see here. Go on....
    if(regs->error_code & VMM_PF_ERRCODE_RSV) {
        return;
    }

    //TODO: Maybe rework logic a little bit?

    //Was the PF caused by a non-present page?
    if(!(regs->error_code & VMM_PF_ERRCODE_PRESENT)) {
        //Is the area mapped in the address space?
            //YES - Is it a dynamic page?
                //YES - grab a page from the PMM, make the mapping and finish the handler
                //NO  - Sus. What other flags in the error code could cause this?
            //NO  - The process is doing something funky, accessing memory not given to it. EXTERMINATE IT (TODO: Scheduler!)

        //Is there an allocated range before the fault_address that overlaps with it?
        struct AVL_node_t* lower_bound = AVL_search_closest_lower(address_space->address_tree->root, fault_address);
        
        //If there is no match, or the one found does not correspond to the fault address, then kill the process.
        if(lower_bound == NULL || !(lower_bound->key <= fault_address && lower_bound->key + lower_bound->range->size >= fault_address)) {
            //Kill process
            return;
        }
        
        fault_address = align(fault_address, lower_bound->range->page_size);

        void* phys = PMM_alloc_aligned(lower_bound->range->page_size, lower_bound->range->page_size);
        if(phys == NULL) {} //Out of memory, kill that process?? or swap, but thats TODO
        //REPLACE MMU_map_range-address_space with the processes' address space, TODO
        int res = MMU_map_range(address_space->CR3, phys, (void*)fault_address, lower_bound->range->page_size, lower_bound->range->page_size, lower_bound->range->flags | MMU_FLAG_PRESENT, 0);
        if(res != 0) {} //There was an error mapping. Kill that process!

        VMM_range_push_physical_page(lower_bound->range, (u64)phys, fault_address, lower_bound->range->page_size);

        //TODO: What if it is a file?
        lower_bound->range->status = VMM_STATUS_RAM_ALLOCATED;

        return;
    }

    //Was the PF caused by a fetch from an NX-page?
    if(regs->error_code & VMM_PF_ERRCODE_ID) {
        //Kill that process!
        return;
    }

    //Was the PF caused by a protection key violation?
    if(regs->error_code & VMM_PF_ERRCODE_PK) {
        //TODO
        return;
    }

    //Was the PF caused by a shadow stack access violation?
    if(regs->error_code & VMM_PF_ERRCODE_SS) {
        //TODO
        return;
    }

    //Something else caused the PF, could be RMP. TODO
    return;
}

//VMM_deleteAddressSpace
//VMM_switchAddressSpace??
//VMM_populateAddressSpace
//VMM_reserve?? -- Kind of like alloc but does more so on a separate non-active virtual address space, whereas VMM_alloc will always access the current address space
//+ something like VMM_swapPage & VMM_bringSwap

//TODO: Handle multiple separate processes trying to allocate the same MMIO / File / etc