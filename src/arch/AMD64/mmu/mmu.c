#include <kernel/debug/log.h>
#include <types.h>
#include <arch/AMD64/mmu/mmu.h>
#include <arch/AMD64/cpu/cpu.h>
#include <kernel/mm/pmm.h>

#define KERNEL_HIGHER_HALF_ADDR 0xFFFFFFFF80000000

#define PML4_idx(x) ((u64)x >> 39) & 0x1FF
#define PDPT_idx(x) ((u64)x >> 30) & 0x1FF
#define PD_idx(x)   ((u64)x >> 21) & 0x1FF
#define PT_idx(x)   ((u64)x >> 12) & 0x1FF

#define MMU_ENTRY_PRESENT 1
#define MMU_ENTRY_PS 1 << 7
//And possibly more flags, not necessary as of now...

#define align(x, y) ((u64)x & ~((u64)y-1))

#define is_address_lower_half(x) ((u64)x < LOWER_HALF_ADDR)
#define is_address_higher_half(x) ((u64)x >= HIGHER_HALF_ADDR)
#define is_address_canonical(x) (is_address_lower_half(x) || is_address_higher_half(x))
#define page_to_paddr(x) (void*)((u64)x & ~((1 << 12) - 1) & (((u64)1 << paddr_length) - 1))

static bool PAGE_1GB_SUPPORTED = false;
static u8 vaddr_length; //Store the implemented virtual address bits for canonical address checking
static u8 paddr_length;
static u64 LOWER_HALF_ADDR = 0; //The end of the lower memory half
static u64 HIGHER_HALF_ADDR = 0; //The start of the higher memory half

//PT
struct PTe_t {
    u64 raw;
};

struct PT_t {
    struct PTe_t entries[512];
};


//PD
struct PDe_t {
    u64 raw;
};

struct PD_t {
    union {
        struct PT_t* PT[512];
        struct PDe_t entries[512];
    };
};

//PDPT
struct PDPe_t {
    u64 raw;
};

struct PDPT_t {
    union {
        struct PD_t* PD[512];
        struct PDPe_t entries[512];
    };
};

//PML4
struct PML4e_t {
    u64 raw;
};

struct PML4_t {
    union {
        struct PDPT_t* PDPT[512];
        struct PML4e_t entries[512];
    };
};

//
// Maybe PML5?
//

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

        LOWER_HALF_ADDR = ((u64)1 << (vaddr_length-1)) - 1;
        HIGHER_HALF_ADDR = (~(u64)0 ^ LOWER_HALF_ADDR);

        address_spaces.addr = PML4;
        address_spaces.cpuId = X86_CPU_get_cpuid();

        u32 edx = 0;
        X86_CPU_cpuid(0x80000001, &unused, &unused, &unused, &edx);
        PAGE_1GB_SUPPORTED = edx & (1 << 26);
    }

    struct address_spaces_t* address_space = &address_spaces;
    while(address_space->next) address_space = address_space->next;
    //alloc space for the new address space

    address_space->addr = PML4;
    address_space->cpuId = X86_CPU_get_cpuid();
    return;
}

void *MMU_get_cr3() {
    void *cr3 = 0;
    asm volatile("mov %%cr3, %0" : "=r" (cr3) : : "rax");
    return cr3;
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

inline bool MMU_is_canonical(void *addr) {
    return is_address_canonical(addr);
}

//
// Returns 0 when address is in lower half
// Returns 1 when address is in higher half
// Returns 2 when address is in kernel space (>=0xFFFFFFFF80000000)
// Returns -1 When address is not canonical
//
inline int MMU_get_address_half(void *addr) {
    if(is_address_lower_half(addr)) return MMU_addr_lower_half;
    if(is_address_higher_half(addr)) {
        if((u64)addr < KERNEL_HIGHER_HALF_ADDR) return MMU_addr_higher_half;
        return MMU_addr_kernel_half;
    }
    return -1;
}

inline void *MMU_make_addr_half(void *addr, int half) {
    if(!is_address_canonical(addr)) return NULL;
    switch(half) {
        case MMU_addr_lower_half:
            return (void*)((u64)addr & ~KERNEL_HIGHER_HALF_ADDR);
        case MMU_addr_higher_half:
            return (void*)(((u64)addr & ~KERNEL_HIGHER_HALF_ADDR) | HIGHER_HALF_ADDR);
        case MMU_addr_kernel_half:
            return (void*)((u64)addr | KERNEL_HIGHER_HALF_ADDR);
        default:
            return NULL;
    }
}

//The following functions are helpers to map the flags into the entry fields, since bitfields are slower and honestly just bad.

#define flag_n(x, flag, n) (int)((bool)(x & flag)) * n

inline void set_PTe(struct PTe_t *entry, u64 addr, u32 flags, u16 Available) {
    entry->raw = addr | flag_n(flags, MMU_FLAG_NX, (u64)1 << 63) | flag_n(flags, MMU_FLAG_GLOBAL, 1 << 8) | flag_n(flags, MMU_FLAG_PAT, 1 << 7) | flag_n(flags, MMU_FLAG_PCD, 1 << 4) | flag_n(flags, MMU_FLAG_PWT, 1 << 3) | flag_n(flags, MMU_FLAG_SUPERVISOR, 1 << 2) | flag_n(flags, MMU_FLAG_RW, 1 << 1) | flag_n(flags, MMU_FLAG_PRESENT, 1);
    entry->raw |= (Available & ((1 << 4) - 1)) << 9 | (u64)((Available >> 3) & ((1 << 12) - 1)) << 52;
}

inline void set_PDe(struct PDe_t *entry, u64 addr, u32 flags, u16 Available) {
    entry->raw = addr | flag_n(flags, MMU_FLAG_NX, (u64)1 << 63) | flag_n(flags, MMU_FLAG_PAT, 1 << 12) | flag_n(flags, MMU_FLAG_GLOBAL, 1 << 8) | 1 << 7 | flag_n(flags, MMU_FLAG_PCD, 1 << 4) | flag_n(flags, MMU_FLAG_PWT, 1 << 3) | flag_n(flags, MMU_FLAG_SUPERVISOR, 1 << 2) | flag_n(flags, MMU_FLAG_RW, 1 << 1) | flag_n(flags, MMU_FLAG_PRESENT, 1);
    entry->raw |= (Available & ((1 << 4) - 1)) << 9 | (u64)((Available >> 3) & ((1 << 12) - 1)) << 52;
}

inline void set_PDPTe(struct PDPe_t *entry, u64 addr, u32 flags, u16 Available) {
    entry->raw = addr | flag_n(flags, MMU_FLAG_NX, (u64)1 << 63) | flag_n(flags, MMU_FLAG_PAT, 1 << 12) | flag_n(flags, MMU_FLAG_GLOBAL, 1 << 8) | 1 << 7 | flag_n(flags, MMU_FLAG_PCD, 1 << 4) | flag_n(flags, MMU_FLAG_PWT, 1 << 3) | flag_n(flags, MMU_FLAG_SUPERVISOR, 1 << 2) | flag_n(flags, MMU_FLAG_RW, 1 << 1) | flag_n(flags, MMU_FLAG_PRESENT, 1);
    entry->raw |= (Available & ((1 << 4) - 1)) << 9 | (u64)((Available >> 3) & ((1 << 12) - 1)) << 52;
}


//
// 
// Returns 0 when succesfully mapped
// Returns 1 When a parameter is wrong
// Returns 2 when paddr or vaddr is not aligned
// Returns 3 when virtual address was already mapped
// 
// 

u8 MMU_map_page(void *address_space, void *paddr, void *vaddr, u32 page_size, u32 flags) {
    if (address_space == NULL) address_space = MMU_get_cr3();

    if(page_size != MMU_PAGE_4K && page_size != MMU_PAGE_2M && page_size != MMU_PAGE_1G) return 1;
    if(page_size == MMU_PAGE_1G && !PAGE_1GB_SUPPORTED) return 1;
    if(!is_address_canonical(vaddr)) return 1;

    if((u64)paddr != align(paddr, page_size) || (u64)vaddr != align(vaddr, page_size)) return 2;

    u16 PML4_idx = PML4_idx(vaddr);
    u16 PDPT_idx = PDPT_idx(vaddr);
    u16 PD_idx = PD_idx(vaddr);
    u16 PT_idx = PT_idx(vaddr);

    struct PML4_t* PML4 = (struct PML4_t*)address_space;
    if (!PML4->entries[PML4_idx].raw) {
        PML4->entries[PML4_idx].raw = (u64)PMM_alloc_aligned(MMU_PAGE_4K, 0); //Allocate memory for the table
        PML4->entries[PML4_idx].raw |= MMU_FLAG_PRESENT | MMU_FLAG_SUPERVISOR | MMU_FLAG_RW; //Set Present, Supervisor and rw bits with the logic in mind that only the lowest entry in the chain sets the actual privileges
    }

    struct PDPT_t* PDPT = page_to_paddr(PML4->PDPT[PML4_idx]);
    if (page_size == MMU_PAGE_1G) {
        if(page_to_paddr(PDPT->entries[PDPT_idx].raw)) return 3;
        set_PDPTe(&PDPT->entries[PDPT_idx], (u64)paddr, flags, 0);
        return 0;
    }

    if (!PDPT->entries[PDPT_idx].raw) {
        PDPT->entries[PDPT_idx].raw = (u64)PMM_alloc_aligned(MMU_PAGE_4K, 0);
        PDPT->entries[PDPT_idx].raw |= MMU_FLAG_PRESENT | MMU_FLAG_SUPERVISOR | MMU_FLAG_RW;
    }

    struct PD_t* PD = page_to_paddr(PDPT->PD[PDPT_idx]);
    if (page_size == MMU_PAGE_2M) {
        if(page_to_paddr(PD->entries[PD_idx].raw)) return 3;
        set_PDe(&PD->entries[PD_idx], (u64)paddr, flags, 0);
        return 0;
    }

    if (!PD->entries[PD_idx].raw) {
        PD->entries[PD_idx].raw = (u64)PMM_alloc_aligned(MMU_PAGE_4K, 0);
        PD->entries[PD_idx].raw |= MMU_FLAG_PRESENT | MMU_FLAG_SUPERVISOR | MMU_FLAG_RW;
    }

    struct PT_t* PT = page_to_paddr(PD->PT[PD_idx]);
    if(page_to_paddr(PT->entries[PT_idx].raw)) return 3;
    set_PTe(&PT->entries[PT_idx], (u64)paddr, flags, 0);

    return 0;
}

u8 MMU_map_range(void *address_space, void *paddr, void *vaddr, u64 size, u32 page_size, u32 flags) {
    while(size) {
        DEBUG_log((const string)"MMU_map_range; paddr: %x, vaddr: %x, page size: %x, size: %x\0", paddr, vaddr, page_size, size);

        if(size < page_size) {
            size = page_size;
            page_size = MMU_PAGE_4K;
        }
        u8 res = MMU_map_page(address_space, paddr, vaddr, page_size, flags);
        if (res != 0) return res; //TODO: And also possibly unmap everything that was just mapped..
        paddr = (void*)((u64)paddr + page_size);
        vaddr = (void*)((u64)vaddr + page_size);

        size -= page_size;
    }

    return 0;
}

void* MMU_get_paddr(void* address_space, void* vaddr) {
    if(!is_address_canonical(vaddr)) return (void*)-1;

    if(address_space == NULL) address_space = MMU_get_cr3();

    u16 PML4_idx = PML4_idx(vaddr);
    u16 PDPT_idx = PDPT_idx(vaddr);
    u16 PD_idx = PDPT_idx(vaddr);
    u16 PT_idx = PDPT_idx(vaddr);

    struct PML4_t* PML4 = (struct PML4_t*)address_space;
    if (!(PML4->entries[PML4_idx].raw & MMU_ENTRY_PRESENT)) return NULL;

    struct PDPT_t* PDPT = PML4->PDPT[PML4_idx];
    if (!(PDPT->entries[PDPT_idx].raw & MMU_ENTRY_PRESENT)) return NULL;
    if (PDPT->entries[PDPT_idx].raw & MMU_ENTRY_PS) return page_to_paddr(PDPT->entries[PDPT_idx].raw);

    struct PD_t* PD = PDPT->PD[PDPT_idx];
    if (!(PD->entries[PD_idx].raw & MMU_ENTRY_PRESENT)) return NULL;
    if (PD->entries[PD_idx].raw & MMU_ENTRY_PS) return page_to_paddr(PD->entries[PD_idx].raw);

    struct PT_t* PT = PD->PT[PD_idx];
    if (!(PT->entries[PT_idx].raw & MMU_ENTRY_PRESENT)) return NULL;
    return page_to_paddr(PT->entries[PT_idx].raw);
}

//TODO: Unmap