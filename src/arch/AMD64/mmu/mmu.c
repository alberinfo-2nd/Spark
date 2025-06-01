#include <types.h>
#include <arch/AMD64/mmu/mmu.h>
#include <arch/AMD64/cpu/cpu.h>

#define PML4_idx(x) ((u64)x >> 39) & 0x1FF
#define PDPT_idx(x) ((u64)x >> 30) & 0x1FF
#define PD_idx(x)   ((u64)x >> 21) & 0x1FF
#define PT_idx(x)   ((u64)x >> 12) & 0x1FF

#define align(x, y) ((u64)x & ~(y-1))

#define is_address_canonical(x) ((u64)x < (u64)1 << vaddr_length && (u64)x > (~(u64)0 ^ (1 << vaddr_length)))
#define page_to_paddr(x) (void*)((u64)x & ~((1 << 12) - 1) & ((1 << paddr_length) - 1))

static u8 vaddr_length; //Store the implemented virtual address bits for canonical address checking
static u8 paddr_length;

struct PTe_t {
    union {
        struct {
            u8 NX : 1;
            u32 Available : 11;
            u64 : 40;
            u8 AVL : 3;
            u8 Global : 1;
            u8 PAT : 1;
            u8 Dirty : 1;
            u8 Accessed : 1;
            u8 PCD : 1;
            u8 PWT : 1;
            u8 Supervisor : 1;
            u8 RW : 1;
            u8 Present : 1;
        } bits __attribute__((packed));
        u64 raw;
    };
};

struct PT_t {
    struct PTe_t entries[512];
};


struct PDe_t {
    union {
        struct {
            u8 NX : 1;
            u32 Available : 11;
            u64 : 39;
            u8 PAT : 1;
            u8 AVL : 3;
            u8 Global : 1;
            u8 PS : 1; //Is this entry the Lowest Level, or is the address pointing to a PT?
            u8 Dirty : 1;
            u8 Accessed : 1;
            u8 PCD : 1;
            u8 PWT : 1;
            u8 Supervisor : 1;
            u8 RW : 1;
            u8 Present : 1;
        } bits __attribute__((packed));
        u64 raw;
    };
};

struct PD_t {
    union {
        struct PT_t** PT;
        struct PDe_t entries[512];
    };
};


struct PDPe_t {
    union {
        struct {
            u8 NX : 1;
            u32 Available : 11;
            u64 : 39;
            u8 PAT : 1;
            u8 AVL : 3;
            u8 Global : 1;
            u8 PS : 1; //Is this entry the Lowest Level, or is the address pointing to a PT?
            u8 Dirty : 1;
            u8 Accessed : 1;
            u8 PCD : 1;
            u8 PWT : 1;
            u8 Supervisor : 1;
            u8 RW : 1;
            u8 Present : 1;
        } bits __attribute__((packed));
        u64 raw;
    };
};

struct PDPT_t {
    union {
        struct PD_t** PD;
        struct PDPe_t entries[512];
    };
};


struct PML4e_t {
    union {
        struct {
            u8 NX : 1;
            u32 Available : 11;
            u64 : 40;
            u8 AVL : 3;
            u8 : 3;
            u8 Accessed : 1;
            u8 PCD : 1;
            u8 PWT : 1;
            u8 Supervisor : 1;
            u8 RW : 1;
            u8 Present : 1;
        } bits __attribute__((packed));
        u64 raw;
    };
};

struct PML4_t {
    union {
        struct PDPT_t** PDPT;
        struct PML4e_t entries[512];
    };
};

//Stores the pointer to the current cr3 in each thread / core
struct address_spaces_t {
    struct PML4_t* addr;
    struct address_spaces_t* next;
    u32 cpuId;
};

//List of current CR3 per cpu
struct address_spaces_t address_spaces;

void MMU_init(void *PML4) {
    //First entry is empty
    if(address_spaces.addr == NULL) {
        u32 eax = 0, unused = 0;
        X86_CPU_cpuid(0x80000008, &eax, &unused, &unused, &unused);
        paddr_length = eax & 0xFF;
        vaddr_length = (eax >> 8) & 0xFF;

        address_spaces.addr = PML4;
        address_spaces.cpuId = X86_CPU_get_cpuid();
    }

    struct address_spaces_t* address_space = &address_spaces;
    while(address_space->next) address_space = address_space->next;
    //alloc space for the new address space

    address_space->addr = PML4;
    address_space->cpuId = X86_CPU_get_cpuid();
    return;
}

void MMU_switch_cr3(void *PML4) {
    if (!is_address_canonical(PML4)) return;

    u8 current_cpuid = X86_CPU_get_cpuid();
    for(struct address_spaces_t* address_space = &address_spaces; address_space; address_space = address_space->next) {
        if(address_space->cpuId != current_cpuid) continue;

        address_space->addr = (struct PML4_t*)PML4;
        break;
    }

    X86_CPU_set_cr3(PML4);
}

//
// 
// Returns 0 when succesfully mapped
// Returns 1 When a parameter is wrong
// Returns 2 when paddr or vaddr is not aligned
// Returns 3 when address was already mapped
// 
// 

u8 MMU_map(void *address_space, void *paddr, void *vaddr, u32 size, u32 flags) {
    if(size != MMU_PAGE_4K && size != MMU_PAGE_2M && size != MMU_PAGE_1G) return 1;
    if(!is_address_canonical(vaddr)) return 1;
    if((u64)paddr != align(paddr, size) || (u64)vaddr != align(vaddr, size)) return 2;

    u16 PML4_idx = PML4_idx(vaddr);
    u16 PDPT_idx = PDPT_idx(vaddr);
    u16 PD_idx = PDPT_idx(vaddr);
    u16 PT_idx = PDPT_idx(vaddr);

    struct PML4_t* PML4 = (struct PML4_t*)address_space;
    if (!PML4->entries[PML4_idx].raw) {
        //Allocate memory for the table

        //Set Present, Supervisor and rw bits with the logic in mind that only the lowest entry in the chain sets the actual privileges
        PML4->entries[PML4_idx].raw |= MMU_FLAG_PRESENT | MMU_FLAG_SUPERVISOR | MMU_FLAG_SUPERVISOR;
    }

    struct PDPT_t* PDPT = PML4->PDPT[PML4_idx];    
    if (size == MMU_PAGE_1G) {
        if(PDPT->entries[PDPT_idx].raw) return 3;
        PDPT->entries[PDPT_idx].raw = (u64)paddr << 12;

        PDPT->entries[PDPT_idx].bits.Present = flags & MMU_FLAG_PRESENT;
        PDPT->entries[PDPT_idx].bits.Supervisor = flags & MMU_FLAG_SUPERVISOR;
        PDPT->entries[PDPT_idx].bits.RW = flags & MMU_FLAG_RW;
        PDPT->entries[PDPT_idx].bits.PWT = flags & MMU_FLAG_PWT;
        PDPT->entries[PDPT_idx].bits.PCD = flags & MMU_FLAG_PCD;
        PDPT->entries[PDPT_idx].bits.PS = true;
        PDPT->entries[PDPT_idx].bits.PAT = flags & MMU_FLAG_PAT;
        PDPT->entries[PDPT_idx].bits.Global = flags & MMU_FLAG_GLOBAL;
        PDPT->entries[PDPT_idx].bits.NX = flags & MMU_FLAG_NX;
        return 0;
    }

    if (!PDPT->entries[PDPT_idx].raw) {
        //Allocate memory for the table

        PDPT->entries[PDPT_idx].raw |= MMU_FLAG_PRESENT | MMU_FLAG_SUPERVISOR | MMU_FLAG_SUPERVISOR;
    }

    struct PD_t* PD = PDPT->PD[PDPT_idx];
    if (size == MMU_PAGE_2M) {
        if(PD->entries[PD_idx].raw) return 3;
        PD->entries[PD_idx].raw = (u64)paddr << 12;
        
        PD->entries[PD_idx].bits.Present = flags & MMU_FLAG_PRESENT;
        PD->entries[PD_idx].bits.Supervisor = flags & MMU_FLAG_SUPERVISOR;
        PD->entries[PD_idx].bits.RW = flags & MMU_FLAG_RW;
        PD->entries[PD_idx].bits.PWT = flags & MMU_FLAG_PWT;
        PD->entries[PD_idx].bits.PCD = flags & MMU_FLAG_PCD;
        PD->entries[PD_idx].bits.PS = true;
        PD->entries[PD_idx].bits.PAT = flags & MMU_FLAG_PAT;
        PD->entries[PD_idx].bits.Global = flags & MMU_FLAG_GLOBAL;
        PD->entries[PD_idx].bits.NX = flags & MMU_FLAG_NX;
        return 0;
    }

    if (!PD->entries[PD_idx].raw) {
        //Allocate memory for the table

        PD->entries[PD_idx].raw |= MMU_FLAG_PRESENT | MMU_FLAG_SUPERVISOR | MMU_FLAG_SUPERVISOR;
    }
    
    struct PT_t* PT = PD->PT[PD_idx];
    if(PT->entries[PT_idx].raw) return 3;
    PT->entries[PT_idx].raw = (u64)paddr << 12;
    
    PT->entries[PT_idx].bits.Present = flags & MMU_FLAG_PRESENT;
    PT->entries[PT_idx].bits.Supervisor = flags & MMU_FLAG_SUPERVISOR;
    PT->entries[PT_idx].bits.RW = flags & MMU_FLAG_RW;
    PT->entries[PT_idx].bits.PWT = flags & MMU_FLAG_PWT;
    PT->entries[PT_idx].bits.PCD = flags & MMU_FLAG_PCD;
    PT->entries[PT_idx].bits.PAT = flags & MMU_FLAG_PAT;
    PT->entries[PT_idx].bits.Global = flags & MMU_FLAG_GLOBAL;
    PT->entries[PT_idx].bits.NX = flags & MMU_FLAG_NX;
    return 0;
}

void* MMU_get_paddr(void* address_space, void* vaddr) {
    if(!is_address_canonical(vaddr)) return (void*)-1;

    u16 PML4_idx = PML4_idx(vaddr);
    u16 PDPT_idx = PDPT_idx(vaddr);
    u16 PD_idx = PDPT_idx(vaddr);
    u16 PT_idx = PDPT_idx(vaddr);

    struct PML4_t* PML4 = (struct PML4_t*)address_space;
    if (!PML4->entries[PML4_idx].bits.Present) return NULL;

    struct PDPT_t* PDPT = PML4->PDPT[PML4_idx];
    if (!PDPT->entries[PDPT_idx].bits.Present) return NULL;
    if (PDPT->entries[PDPT_idx].bits.PS) return page_to_paddr(PDPT->entries[PDPT_idx].raw);

    struct PD_t* PD = PDPT->PD[PDPT_idx];
    if (!PD->entries[PD_idx].bits.Present) return NULL;
    if (PD->entries[PD_idx].bits.PS) return page_to_paddr(PD->entries[PD_idx].raw);

    struct PT_t* PT = PD->PT[PD_idx];
    if (!PT->entries[PT_idx].bits.Present) return NULL;
    return page_to_paddr(PT->entries[PT_idx].raw);
}