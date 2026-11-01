#ifndef BOOT_MULTIBOOT2_H
#define BOOT_MULTIBOOT2_H

#include <stddef.h>
#include <stdint.h>

#define MULTIBOOT2_BOOTLOADER_MAGIC 0x36D76289

#define MULTIBOOT_TAG_TYPE_END              0
#define MULTIBOOT_TAG_TYPE_CMDLINE          1
#define MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME 2
#define MULTIBOOT_TAG_TYPE_MMAP             6

#define MULTIBOOT_MEMORY_AVAILABLE          1
#define MULTIBOOT_MEMORY_RESERVED           2
#define MULTIBOOT_MEMORY_ACPI_RECLAIMABLE   3
#define MULTIBOOT_MEMORY_NVS                4
#define MULTIBOOT_MEMORY_BADRAM             5

struct multiboot_tag {
    uint32_t type;
    uint32_t size;
};

struct multiboot_tag_string {
    uint32_t type;
    uint32_t size;
    char string[0];
};

struct multiboot_mmap_entry {
    uint64_t addr;
    uint64_t len;
    uint32_t type;
    uint32_t zero;
} __attribute__((packed));

struct multiboot_tag_mmap {
    uint32_t type;
    uint32_t size;
    uint32_t entry_size;
    uint32_t entry_version;
    struct multiboot_mmap_entry entries[0];
} __attribute__((packed));

/* Total size in bytes of the boot information block (including its header). */
uint32_t mb2_info_size(uint32_t mb_info_addr);

/* First tag of the given type, or NULL if the bootloader did not provide one. */
const struct multiboot_tag *mb2_find_tag(uint32_t mb_info_addr, uint32_t type);

const struct multiboot_tag_mmap *mb2_find_mmap_tag(uint32_t mb_info_addr);

/* Bootloader name tag text (e.g. "GRUB 2.12"), or NULL if absent. */
const char *mb2_bootloader_name(uint32_t mb_info_addr);

/* Walk every tag: first = mb2_first_tag(addr); next = mb2_next_tag(addr, cur). */
const struct multiboot_tag *mb2_first_tag(uint32_t mb_info_addr);
const struct multiboot_tag *mb2_next_tag(uint32_t mb_info_addr,
                                         const struct multiboot_tag *tag);
const char *mb2_tag_type_name(uint32_t type);

/* Human-readable name for a MULTIBOOT_MEMORY_* region type. */
const char *mb2_mmap_type_name(uint32_t type);

#endif
