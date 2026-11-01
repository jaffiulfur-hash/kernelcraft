#include "boot/multiboot2.h"

uint32_t mb2_info_size(uint32_t mb_info_addr) {
    return *(const uint32_t *)(uintptr_t)mb_info_addr;
}

const struct multiboot_tag *mb2_find_tag(uint32_t mb_info_addr, uint32_t type) {
    const uint8_t *base = (const uint8_t *)(uintptr_t)mb_info_addr;
    const uint8_t *end = base + mb2_info_size(mb_info_addr);
    const uint8_t *ptr = base + 8;   /* skip total_size + reserved */

    /* Every tag starts with {type, size}; guard against a truncated block. */
    while (ptr + sizeof(struct multiboot_tag) <= end) {
        const struct multiboot_tag *tag = (const struct multiboot_tag *)ptr;

        if (tag->type == MULTIBOOT_TAG_TYPE_END) {
            break;
        }

        if (tag->type == type) {
            return tag;
        }

        if (tag->size < sizeof(struct multiboot_tag)) {
            break;   /* corrupt tag, would loop forever */
        }

        /* Tags are padded so that the next one starts 8-byte aligned. */
        ptr += (tag->size + 7u) & ~7u;
    }

    return 0;
}

const struct multiboot_tag_mmap *mb2_find_mmap_tag(uint32_t mb_info_addr) {
    return (const struct multiboot_tag_mmap *)
        mb2_find_tag(mb_info_addr, MULTIBOOT_TAG_TYPE_MMAP);
}

const char *mb2_bootloader_name(uint32_t mb_info_addr) {
    const struct multiboot_tag_string *tag =
        (const struct multiboot_tag_string *)
            mb2_find_tag(mb_info_addr, MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME);

    return tag ? tag->string : 0;
}

const char *mb2_mmap_type_name(uint32_t type) {
    switch (type) {
        case MULTIBOOT_MEMORY_AVAILABLE:        return "available";
        case MULTIBOOT_MEMORY_RESERVED:         return "reserved";
        case MULTIBOOT_MEMORY_ACPI_RECLAIMABLE: return "ACPI reclaimable";
        case MULTIBOOT_MEMORY_NVS:              return "ACPI NVS";
        case MULTIBOOT_MEMORY_BADRAM:           return "bad RAM";
        default:                                return "unknown";
    }
}
