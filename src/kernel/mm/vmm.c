#include <kernel/mm/vmm.h>
#include <kernel/mm/pmm.h>
#include <arch/AMD64/mmu/mmu.h>
#include <kernel/mm/kalloc.h>
#include <kernel/debug/log.h>
#include <arch/AMD64/cpu/cpu.h>

#define max(a, b) (a > b ? a : b)
#define nodeHeight(node) (node ? node->height : -1)
#define balance(node) (nodeHeight(node->child[AVL_CHILD_LEFT]) - nodeHeight(node->child[AVL_CHILD_RIGHT]))
#define leftLeaning(node) (balance(node) > 1)
#define rightLeaning(node) (balance(node) < -(i8)1)

struct Address_space_range_t {
    struct AVL_node_t* addressNode;
    struct AVL_node_t* sizeNode;
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
struct AVL_node_t* AVL_search_closest(struct AVL_node_t* node, u64 key) {
    if(node == NULL) return NULL;
    struct AVL_node_t* best_match = node;

    while(node) {
        if(node->key <= best_match->key && node->key >= key) best_match = node;

        if(node->key == key) return node;
        node = node->child[AVL_CHILD_RIGHT - (int)(node->key > key)];
    }

    return best_match;
}

struct AVL_node_t* AVL_get_minimum(struct AVL_node_t* node) {
    while(node->child[AVL_CHILD_LEFT]) node = node->child[AVL_CHILD_LEFT];

    return node;
}

struct AVL_node_t* AVL_get_successor(struct AVL_node_t* node) {
    if(node->child[AVL_CHILD_RIGHT]) return AVL_get_minimum(node->child[AVL_CHILD_RIGHT]);
    while(node->parent && node->parent->child[AVL_CHILD_RIGHT] == node) node = node->parent; //This can be null if the current node is the rightmost node (No successor)

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

void VMM_add_range(struct VMM_Address_Space_t* address_space, u64 address, u64 size) {
    struct Address_space_range_t* range = kalloc(sizeof(struct Address_space_range_t));
    range->addressNode = AVL_insert(address_space->address_tree, range, address);
    range->sizeNode = AVL_insert(address_space->size_tree, range, size);
}

void VMM_init(void) {
    struct VMM_Address_Space_t* address_space = kalloc(sizeof(struct VMM_Address_Space_t));
    address_space->CR3 = MMU_get_cr3();
    address_space->address_tree = kalloc(sizeof(struct AVL_tree_t));
    address_space->size_tree = kalloc(sizeof(struct AVL_tree_t));
    
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
    address_space->size_tree = kalloc(sizeof(struct AVL_tree_t));

    //TODO: COPY CR3!!!!!
    //Populates the address space
    MMU_travel_address_space(address_space);
    return address_space;
}

struct VMM_Address_Space_t* VMM_get_current_address_space(void) {
    return X86_CPU_get_self()->address_space;
}

//Allocates n bytes within the provided address space. Each memory type has some default options enforced
//(Such as being read-only, MMIO being uncacheable, etc), which may be overridden by the flags parameter (uses MMU_FLAG_*)
//page_size uses MMU_PAGE_X as size
void* VMM_alloc(struct VMM_Address_Space_t* address_space, u64 size, u8 type, u32 flags, u32 page_size) {
    if((size & ~(MMU_PAGE_4K-1)) != size) return NULL; //We dont deal with sizes that are not aligned to at least a 4KiB page, or smaller than that

    if(type > VMM_TYPE_RESERVED) return NULL; //Unknown VMM_TYPE

    if(address_space == NULL) address_space = VMM_get_current_address_space(); //Maybe also check if the address space is valid / real?

    struct AVL_node_t* node = AVL_search_closest(address_space->size_tree->root, size);
    if(node == NULL) return NULL; //NO SPACE!!!!

    //Delete-reinsert, but check if that is needed 100% of the time
    struct Address_space_range_t* range = node->range;
    u64 newsize = node->key - size; //AVL_search_closest will always return a key that is >= to size 
    u64 newaddress = range->addressNode->key + size;
    void* ptr = (void*)range->addressNode->key;

    AVL_delete(address_space->size_tree, node);
    range->sizeNode = AVL_insert(address_space->size_tree, range, newsize); //Size of this range is equal to (previous size - allocated size)

    AVL_delete(address_space->address_tree, range->addressNode);
    range->addressNode = AVL_insert(address_space->address_tree, range, newaddress);

    switch(type) {
        case VMM_TYPE_DYNAMIC:
            break;
        case VMM_TYPE_BACKED:
            void* phys = PMM_alloc_aligned(size, page_size);
            if(phys == NULL) {} //Out of memory, I guess. What do we do?
            int res = MMU_map_range(NULL, phys, ptr, size, page_size, flags, type);
            if(res != 0) {} //There was an error mapping. What do we do?
            break;
        default:
            //Either does not go here or its not implemented yet
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

int VMM_free(struct VMM_Address_Space_t *address_space, void *ptr, u64 size) {
    if((size & ~(MMU_PAGE_4K-1)) != size) return 1;
    if(address_space == NULL) address_space = VMM_get_current_address_space();

    AVL_trasverse_inorder(address_space->address_tree->root);

    struct AVL_node_t* higher_bound = AVL_search_closest(address_space->address_tree->root, (u64)ptr+size);
    struct AVL_node_t* lower_bound = AVL_search_closest(address_space->address_tree->root, (u64)ptr);

    if(higher_bound && higher_bound->key <= (u64)ptr+size) return 1; //Its not possible for either ptr or size to be right, because there is a free address range starting before the range provided to VMM_free
    if(lower_bound && lower_bound->key + lower_bound->range->sizeNode->key >= (u64)ptr && lower_bound->key <= (u64)ptr) return 1; //Same as above but for the lower end

    VMM_add_range(address_space, (u64)ptr, size);

    return 0;
}

//VMM_deleteAddressSpace
//VMM_switchAddressSpace??
//VMM_populateAddressSpace
//VMM_reserve?? -- Kind of like alloc but does more so on a separate non-active virtual address space, whereas VMM_alloc will always access the current address space
//+ something like VMM_swapPage & VMM_bringSwap

//TODO: How do you keep track of which physical addresses are reserved to a virtual address?
// such that you dont have to go on (at worst) 4KiB increments and perform a table walk for each step in the range...

//TODO: Merge addresses, maybe checking with a scheduler when there is free time to do so?