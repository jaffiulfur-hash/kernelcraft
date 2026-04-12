#include "boot/multiboot2.h"

const struct multiboot_tag_mmap *mb2_find_mmap_tag(uint32_t mb_info_addr) {
    uint8_t *base = (uint8_t *)(uintptr_t)mb_info_addr;
    uint32_t total_size = *(uint32_t *)base;
    uint8_t *ptr = base + 8;
    uint8_t *end = base + total_size;

    while (ptr < end) {
        struct multiboot_tag *tag = (struct multiboot_tag *)ptr;

        if (tag->type == MULTIBOOT_TAG_TYPE_END) {
            break;
        }

        if (tag->type == MULTIBOOT_TAG_TYPE_MMAP) {
            return (const struct multiboot_tag_mmap *)tag;
        }

        ptr += (tag->size + 7u) & ~7u;
    }

    return 0;
}
