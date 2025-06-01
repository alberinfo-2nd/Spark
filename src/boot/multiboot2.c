#include <boot/multiboot2.h>
#include <kernel/mm/pmm.h>

#define MBOOT_TAG_TYPE_END 0
#define MBOOT_TAG_TYPE_MMAP 6

extern u8 KERNEL_LMA_END;

struct MBOOT_TAG_initial_t {
    u32 total_size;
    u32 reserved;
};

struct MBOOT_TAG_t {
    u32 type;
    u32 size;
};

struct MBOOT_mmap_t {
    u64 base_addr;
    u64 length;
    u32 type;
#define MBOOT_MEMORY_AVAILABLE              1
#define MBOOT_MEMORY_RESERVED               2
#define MBOOT_MEMORY_ACPI_RECLAIMABLE       3
#define MBOOT_MEMORY_NVS                    4
#define MBOOT_MEMORY_BADRAM                 5
    u32 reserved;
};

struct MBOOT_TAG_mmap_t {
    u32 type; // = 6
    u32 size;
    u32 entry_size;
    u32 entry_version;
    struct MBOOT_mmap_t entries[0];
};

bool scan_mboot(void *multiboot_data) {
    if ((u64)multiboot_data & 7) return false; //MBI is unaligned, cannot parse multiboot header.

    for (struct MBOOT_TAG_t *tag = (struct MBOOT_TAG_t*)((u64)multiboot_data + sizeof(struct MBOOT_TAG_initial_t)); tag->type != MBOOT_TAG_TYPE_END; tag = (struct MBOOT_TAG_t*)((u64)tag + ((tag->size + 7) & ~(u64)7))) {
        switch (tag->type) {
            case MBOOT_TAG_TYPE_MMAP:
                u32 entry_size = ((struct MBOOT_TAG_mmap_t*)tag)->entry_size;
                for (struct MBOOT_mmap_t* mmap = ((struct MBOOT_TAG_mmap_t*)tag)->entries; (u64)mmap < (u64)tag + tag->size; mmap = (struct MBOOT_mmap_t*)((u64)mmap + entry_size)) {
                    //Only one that could also be added is ACPI_RECLAIMABLE memory, but that would only be the case once we already parsed the ACPI tables and so on, so not happening anytime soon and maybe not even worth doing to begin with
                    if (mmap->type == MBOOT_MEMORY_AVAILABLE) {
                        if(mmap->base_addr < (u64)&KERNEL_LMA_END) {
                            mmap->base_addr = (u64)&KERNEL_LMA_END;
                            if(mmap->length < (u64)&KERNEL_LMA_END) continue;
                            mmap->length -= (u64)&KERNEL_LMA_END;
                        }
                        PMM_add_block((void*)mmap->base_addr, mmap->length);
                    }
                    //The rest of the areas are unusable, except maybe for NVS? which afaik should be saved and restored when going into deep sleep

                }
                break;
        }
    }

    return true;
}