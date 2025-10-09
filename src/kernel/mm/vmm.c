#include <kernel/mm/vmm.h>
#include <kernel/mm/pmm.h>
#include <arch/AMD64/mmu/mmu.h>
#include <kernel/mm/kalloc.h>
#include <kernel/debug/log.h>

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
    struct AVL_node_t* best_match = NULL;

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

//WHAT DO I DO WITH THE NEW ADDRESS SPACE?!?!?! Pass it along to a scheduler, of course...
void VMM_init(void) {
    struct VMM_Address_Space_t* address_space = kalloc(sizeof(struct VMM_Address_Space_t));
    address_space->CR3 = MMU_get_cr3();
    address_space->address_tree = kalloc(sizeof(struct AVL_tree_t));
    address_space->size_tree = kalloc(sizeof(struct AVL_tree_t));
    
    //Populates the address space
    MMU_travel_address_space(address_space);
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

//VMM_getCurrentAddressSpace??
//VMM_deleteAddressSpace
//VMM_switchAddressSpace??
//VMM_populateAddressSpace
//VMM_alloc
//VMM_free
//+ something like VMM_swapPage & VMM_bringSwap

//TODO: Merge addresses, maybe checking with a scheduler when there is free time to do so?