#include <kernel/debug/log.h>
#include <arch/AMD64/mmu/mmu.h>
#include <arch/AMD64/cpu/cpu.h>
#include <kernel/mm/pmm.h>
#include <kernel/mm/vmm.h>

#define KERNEL_HIGHER_HALF_ADDR 0xFFFFFFFF80000000

#define PML4_idx(x) ((u64)x >> 39) & 0x1FF
#define PDPT_idx(x) ((u64)x >> 30) & 0x1FF
#define PD_idx(x)   ((u64)x >> 21) & 0x1FF
#define PT_idx(x)   ((u64)x >> 12) & 0x1FF

#define MMU_ENTRY_PRESENT       1 << 0
#define MMU_ENTRY_RW            1 << 1
#define MMU_ENTRY_SUPERVISOR    1 << 2
#define MMU_ENTRY_PWT           1 << 3
#define MMU_ENTRY_PCD           1 << 4
#define MMU_ENTRY_ACCESSED      1 << 5
#define MMU_ENTRY_DIRTY         1 << 6
#define MMU_ENTRY_PAT           1 << 7
#define MMU_ENTRY_PS            1 << 7
#define MMU_ENTRY_GLOBAL        1 << 8
#define MMU_ENTRY_PAT_BP        1 << 12 //PAT flag on big pages, 2MB and up
#define MMU_ENTRY_NX            1ULL << 63

#define align(x, y) ((u64)x & ~((u64)y-1))

#define is_address_lower_half(x) ((u64)x < LOWER_HALF_ADDR)
#define is_address_higher_half(x) ((u64)x >= HIGHER_HALF_ADDR)
#define is_address_canonical(x) (is_address_lower_half(x) || is_address_higher_half(x))
#define page_to_paddr(x) (void*)((u64)x & ~((1 << 12) - 1) & ((1ULL << paddr_length) - 1))
#define idx_to_vaddr(PML4, PDPT, PD, PT) ((-1ULL << vaddr_length) * (bool)(PML4 & (1 << 8)) | ((u64)PML4 & 0x1FF) << 39 | ((u64)PDPT & 0x1FF) << 30 | ((u64)PD & 0x1FF) << 21 | ((u64)PT & 0x1FF) << 12)

static bool PAGE_1GB_SUPPORTED = false;
static u8 vaddr_length; //Store the implemented virtual address bits for canonical address checking
static u8 paddr_length;
static u64 LOWER_HALF_ADDR = 0; //The end of the lower memory half
static u64 HIGHER_HALF_ADDR = 0; //The start of the higher memory half

struct PageEntry_t {
    u64 raw;
};

struct PT_t {
    struct PageEntry_t entries[512];
};

struct PD_t {
    union {
        struct PT_t* PT[512];
        struct PageEntry_t entries[512];
    };
};

struct PDPT_t {
    union {
        struct PD_t* PD[512];
        struct PageEntry_t entries[512];
    };
};

struct PML4_t {
    union {
        struct PDPT_t* PDPT[512];
        struct PageEntry_t entries[512];
    };
};

//
// Maybe PML5?
//

void MMU_init(void) {
    u32 eax = 0, unused = 0;
    X86_CPU_cpuid(0x80000008, &eax, &unused, &unused, &unused);
    paddr_length = eax & 0xFF;
    vaddr_length = (eax >> 8) & 0xFF;

    LOWER_HALF_ADDR = (1ULL << (vaddr_length-1)) - 1;
    HIGHER_HALF_ADDR = (~0ULL ^ LOWER_HALF_ADDR);

    u32 edx = 0;
    X86_CPU_cpuid(0x80000001, &unused, &unused, &unused, &edx);
    PAGE_1GB_SUPPORTED = edx & (1 << 26);
    return;
}

//Note that this returns the physical address of PML4!!!!
void *MMU_get_cr3() {
    void *cr3 = 0;
    asm volatile("mov %%cr3, %0" : "=r" (cr3) : : "rax");
    return cr3;
}

//The physical address of PML4 is expected. If not given then the switch silently fails.
void MMU_switch_cr3(void *PML4) {
    if (!is_address_canonical(PML4)) return;
    if (MMU_get_address_half(PML4) != MMU_addr_lower_half) return;

    //Setting the same value would just flush the TLB.
    if(MMU_get_cr3() == PML4) return;
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

static inline void set_PTe(struct PageEntry_t *entry, u64 addr, u32 flags, u16 Available) {
    entry->raw = addr | flag_n(flags, MMU_FLAG_NX, MMU_ENTRY_NX) | flag_n(flags, MMU_FLAG_GLOBAL, MMU_ENTRY_GLOBAL) | flag_n(flags, MMU_FLAG_PAT, MMU_ENTRY_PAT) | flag_n(flags, MMU_FLAG_PCD, MMU_ENTRY_PCD) | flag_n(flags, MMU_FLAG_PWT, MMU_ENTRY_PWT) | flag_n(flags, MMU_FLAG_SUPERVISOR, MMU_ENTRY_SUPERVISOR) | flag_n(flags, MMU_FLAG_RW, MMU_ENTRY_RW) | flag_n(flags, MMU_FLAG_PRESENT, MMU_ENTRY_PRESENT);
    entry->raw |= (Available & ((1 << 4) - 1)) << 9 | (u64)((Available >> 3) & ((1 << 12) - 1)) << 52;
}

static inline void set_PDe(struct PageEntry_t *entry, u64 addr, u32 flags, u16 Available) {
    entry->raw = addr | flag_n(flags, MMU_FLAG_NX, MMU_ENTRY_NX) | flag_n(flags, MMU_FLAG_PAT, MMU_ENTRY_PAT_BP) | flag_n(flags, MMU_FLAG_GLOBAL, MMU_ENTRY_GLOBAL) | MMU_ENTRY_PS | flag_n(flags, MMU_FLAG_PCD, MMU_ENTRY_PCD) | flag_n(flags, MMU_FLAG_PWT, MMU_ENTRY_PWT) | flag_n(flags, MMU_FLAG_SUPERVISOR, MMU_ENTRY_SUPERVISOR) | flag_n(flags, MMU_FLAG_RW, MMU_ENTRY_RW) | flag_n(flags, MMU_FLAG_PRESENT, MMU_ENTRY_PRESENT);
    entry->raw |= (Available & ((1 << 4) - 1)) << 9 | (u64)((Available >> 3) & ((1 << 12) - 1)) << 52;
}

static inline void set_PDPTe(struct PageEntry_t *entry, u64 addr, u32 flags, u16 Available) {
    entry->raw = addr | flag_n(flags, MMU_FLAG_NX, MMU_ENTRY_NX) | flag_n(flags, MMU_FLAG_PAT, MMU_ENTRY_PAT_BP) | flag_n(flags, MMU_FLAG_GLOBAL, MMU_ENTRY_GLOBAL) | MMU_ENTRY_PS | flag_n(flags, MMU_FLAG_PCD, MMU_ENTRY_PCD) | flag_n(flags, MMU_FLAG_PWT, MMU_ENTRY_PWT) | flag_n(flags, MMU_FLAG_SUPERVISOR, MMU_ENTRY_SUPERVISOR) | flag_n(flags, MMU_FLAG_RW, MMU_ENTRY_RW) | flag_n(flags, MMU_FLAG_PRESENT, MMU_ENTRY_PRESENT);
    entry->raw |= (Available & ((1 << 4) - 1)) << 9 | (u64)((Available >> 3) & ((1 << 12) - 1)) << 52;
}

//Helpers to free a table when (re)mapping/freeing an address
//TODO: Reducing the amount of invlpg's could be a good optimization. For now they stay for correctness.

static inline void free_PT(u16 PML4_idx, u16 PDPT_idx, u16 PD_idx, struct PT_t* table) {
    for(u16 idx = 0; idx < 512; idx++) {
        if(!table->entries[idx].raw) continue;
        X86_CPU_invlpg((void*)idx_to_vaddr(PML4_idx, PDPT_idx, PD_idx, idx));
    }

    PMM_free(table, MMU_PAGE_4K);
}

static inline void free_PD(u16 PML4_idx, u16 PDPT_idx, struct PD_t* table) {
    for(u16 idx = 0; idx < 512; idx++) {
        if(!table->entries[idx].raw) continue;
        if(table->entries[idx].raw & MMU_ENTRY_PS) X86_CPU_invlpg((void*)idx_to_vaddr(PML4_idx, PDPT_idx, idx, 0));
        else {
            free_PT(PML4_idx, PDPT_idx, idx, page_to_paddr(table->entries[idx].raw));
            table->entries[idx].raw = 0;
        }
    }

    PMM_free(table, MMU_PAGE_4K);
}

static inline void free_PDPT(u16 PML4_idx,struct PDPT_t* table) {
    for(u16 idx = 0; idx < 512; idx++) {
        if(!table->entries[idx].raw) continue;
        if(table->entries[idx].raw & MMU_ENTRY_PS) X86_CPU_invlpg((void*)idx_to_vaddr(PML4_idx, idx, 0, 0));
        else {
            free_PD(PML4_idx, idx, page_to_paddr(table->entries[idx].raw));
            table->entries[idx].raw = 0;
        }
    }

    PMM_free(table, MMU_PAGE_4K);
}

//
// 
// Returns 0 when succesfully mapped
// Returns 1 When a parameter is wrong
// Returns 2 when paddr or vaddr is not aligned
// 
// 

u8 MMU_map_page(void *address_space, void *paddr, void *vaddr, u32 page_size, u32 flags, u16 Available) {
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
        PML4->entries[PML4_idx].raw |= MMU_ENTRY_PRESENT | MMU_ENTRY_SUPERVISOR | MMU_ENTRY_RW; //Set Present, Supervisor and rw bits with the logic in mind that only the lowest entry in the chain sets the actual privileges
    }

    struct PDPT_t* PDPT = page_to_paddr(PML4->PDPT[PML4_idx]);
    if (page_size == MMU_PAGE_1G) {
        free_PDPT(PML4_idx, PDPT);
        set_PDPTe(&PDPT->entries[PDPT_idx], (u64)paddr, flags, Available);
        return 0;
    }

    if (!PDPT->entries[PDPT_idx].raw || PDPT->entries[PDPT_idx].raw & MMU_ENTRY_PS) {
        if(PDPT->entries[PDPT_idx].raw & MMU_ENTRY_PS) X86_CPU_invlpg((void*)idx_to_vaddr(PML4_idx, PDPT_idx, 0, 0));
        PDPT->entries[PDPT_idx].raw = (u64)PMM_alloc_aligned(MMU_PAGE_4K, 0);
        PDPT->entries[PDPT_idx].raw |= MMU_ENTRY_PRESENT | MMU_ENTRY_SUPERVISOR | MMU_ENTRY_RW;
    }

    struct PD_t* PD = page_to_paddr(PDPT->PD[PDPT_idx]);
    if (page_size == MMU_PAGE_2M) {
        free_PD(PML4_idx, PDPT_idx, PD);
        set_PDe(&PD->entries[PD_idx], (u64)paddr, flags, Available);
        return 0;
    }

    if (!PD->entries[PD_idx].raw || PD->entries[PD_idx].raw & MMU_ENTRY_PS) {
        if(PD->entries[PD_idx].raw & MMU_ENTRY_PS) X86_CPU_invlpg((void*)idx_to_vaddr(PML4_idx, PDPT_idx, PD_idx, 0));
        PD->entries[PD_idx].raw = (u64)PMM_alloc_aligned(MMU_PAGE_4K, 0);
        PD->entries[PD_idx].raw |= MMU_ENTRY_PRESENT | MMU_ENTRY_SUPERVISOR | MMU_ENTRY_RW;
    }

    struct PT_t* PT = page_to_paddr(PD->PT[PD_idx]);
    free_PT(PML4_idx, PDPT_idx, PD_idx, PT);
    set_PTe(&PT->entries[PT_idx], (u64)paddr, flags, Available);

    return 0;
}

u8 MMU_map_range(void *address_space, void *paddr, void *vaddr, u64 size, u32 page_size, u32 flags, u16 Available) {
    while(size) {
        if(size < page_size) {
            size = page_size;
            page_size = MMU_PAGE_4K;
        }
        u8 res = MMU_map_page(address_space, paddr, vaddr, page_size, flags, Available);
        if (res != 0) return res; //TODO: And also possibly unmap everything that was just mapped..
        paddr = (void*)((u64)paddr + page_size);
        vaddr = (void*)((u64)vaddr + page_size);

        size -= page_size;
    }

    return 0;
}

//
//
// Returns 1 when vaddr is not aligned
// Otherwise returns size unmapped (MMU_PAGE_SIZE_xx)
// 
// 

//TODO: PMM_free only gets called by helper function when a whole table is freed at once. What if a whole table is freed between different calls?

u64 MMU_unmap_page(void *address_space, void *vaddr) {
    if(address_space == NULL) address_space = MMU_get_cr3();

    //The vaddr should be at least aligned to 4K, though ideally this is checked against the table that actually sets the value
    if((u64)vaddr != align(vaddr, MMU_PAGE_4K)) return 1;

    u16 PML4_idx = PML4_idx(vaddr);
    u16 PDPT_idx = PDPT_idx(vaddr);
    u16 PD_idx = PD_idx(vaddr);
    u16 PT_idx = PT_idx(vaddr);

    struct PML4_t* PML4 = (struct PML4_t*)address_space;
    if (!(PML4->entries[PML4_idx].raw & MMU_ENTRY_PRESENT)) return MMU_PAGE_512G;

    struct PDPT_t* PDPT = page_to_paddr(PML4->PDPT[PML4_idx]);
    if (!(PDPT->entries[PDPT_idx].raw & MMU_ENTRY_PRESENT)) return MMU_PAGE_1G;
    if (PDPT->entries[PDPT_idx].raw & MMU_ENTRY_PS) {
        //vaddr has to be aligned to the page we are trying to unmap, otherwise it makes absolutely no sense.
        if((u64)vaddr != align(vaddr, MMU_PAGE_1G)) return 1;
        PDPT->entries[PDPT_idx].raw = 0;
        X86_CPU_invlpg(vaddr);
        return MMU_PAGE_1G;
    }

    struct PD_t* PD = page_to_paddr(PDPT->PD[PDPT_idx]);
    if (!(PD->entries[PD_idx].raw & MMU_ENTRY_PRESENT)) return MMU_PAGE_2M;
    if (PD->entries[PD_idx].raw & MMU_ENTRY_PS) {
        if((u64)vaddr != align(vaddr, MMU_PAGE_2M)) return 1;
        PD->entries[PD_idx].raw = 0;
        X86_CPU_invlpg(vaddr);
        return MMU_PAGE_2M;
    }

    struct PT_t* PT = page_to_paddr(PD->PT[PD_idx]);
    if (!(PT->entries[PT_idx].raw & MMU_ENTRY_PRESENT)) return MMU_PAGE_4K;
    PT->entries[PT_idx].raw = 0;
    X86_CPU_invlpg(vaddr);

    return MMU_PAGE_4K;
}

//
//
// Returns 1 when vaddr is not aligned to its page_size
// Otherwise returns 0 
// 
// 

u8 MMU_unmap_range(void *address_space, void* vaddr, u64 size) {
    while(size) {
        if(size < MMU_PAGE_4K) size = MMU_PAGE_4K;
        u64 res = MMU_unmap_page(address_space, vaddr);
        if(res == 1) return 1;
        vaddr = (void*)((u64)vaddr + res);
        size -= res;
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

void MMU_travel_address_space(struct VMM_Address_Space_t* address_space) {
    if(address_space == NULL) return;

    const int maxRange = sizeof(struct PT_t)/sizeof(struct PageEntry_t);
    
    struct PML4_t* PML4 = (struct PML4_t*)address_space->CR3;
    u64 rangeAddress = 0;
    u64 rangeSize = 0;
    for(int PML4_idx = 0; PML4_idx < maxRange; PML4_idx++) {
        //If the sign extension between the address range we are currently looking at and the page at PML4[PML4_idx] are different (Lower half vs higher half)
        if (idx_to_vaddr(PML4_idx, 0, 0, 0) >> vaddr_length != rangeAddress >> vaddr_length) {
            if(rangeSize) VMM_add_free_range(address_space, rangeAddress, rangeSize);
            rangeAddress = idx_to_vaddr(PML4_idx, 0, 0, 0);
            rangeSize = 0;
        }

        if(!PML4->entries[PML4_idx].raw) {
            rangeSize += (u64)MMU_PAGE_1G * 512;
            continue;
        }

        /* ------------------------------------------------------ */
        struct PDPT_t* PDPT = page_to_paddr(PML4->entries[PML4_idx].raw);
        for(int PDPT_idx = 0; PDPT_idx < maxRange; PDPT_idx++) {
            if(!PDPT->entries[PDPT_idx].raw) {
                rangeSize += MMU_PAGE_1G;
                continue;
            }

            if(PDPT->entries[PDPT_idx].raw & MMU_ENTRY_PS) {
                if(rangeSize) VMM_add_free_range(address_space, rangeAddress, rangeSize);
                rangeAddress = idx_to_vaddr(PML4_idx, (PDPT_idx+1), 0, 0);
                rangeSize = 0;
                continue;
            }

            /* ------------------------------------------------------ */
            struct PD_t* PD = page_to_paddr(PDPT->entries[PDPT_idx].raw);
            for(int PD_idx = 0; PD_idx < maxRange; PD_idx++) {
                if(!PD->entries[PD_idx].raw) {
                    rangeSize += MMU_PAGE_2M;
                    continue;
                }

                if(PD->entries[PD_idx].raw & MMU_ENTRY_PS) {
                    if(rangeSize) VMM_add_free_range(address_space, rangeAddress, rangeSize);
                    rangeAddress = idx_to_vaddr(PML4_idx, PDPT_idx, (PD_idx+1), 0);
                    rangeSize = 0;
                    continue;
                }

                /* ------------------------------------------------------ */
                struct PT_t* PT = page_to_paddr(PD->entries[PD_idx].raw);
                for(int PT_idx = 0; PT_idx < maxRange; PT_idx++) {
                    if(!PT->entries[PT_idx].raw) {
                        rangeSize += MMU_PAGE_4K;
                        continue;
                    }

                    if(rangeSize) VMM_add_free_range(address_space, rangeAddress, rangeSize);
                    rangeAddress = idx_to_vaddr(PML4_idx, PDPT_idx, PD_idx, (PT_idx+1));
                    rangeSize = 0;
                }
                /* ------------------------------------------------------ */
            }
            /* ------------------------------------------------------ */
        }
        /* ------------------------------------------------------ */
    }

    if(rangeSize) VMM_add_free_range(address_space, rangeAddress, rangeSize);
}

//TODO: MMU_copy_address_space