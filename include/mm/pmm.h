#ifndef MM_PMM_H
#define MM_PMM_H

#include <stddef.h>
#include <stdint.h>
#include "boot/multiboot2.h"

#define PMM_FRAME_SIZE 4096
#define PMM_MAX_MEMORY_BYTES (128ull * 1024ull * 1024ull)
#define PMM_MAX_FRAMES (PMM_MAX_MEMORY_BYTES / PMM_FRAME_SIZE)

void pmm_init(const struct multiboot_tag_mmap *mmap_tag);
uint64_t pmm_alloc_frame(void);
void pmm_free_frame(uint64_t phys_addr);

uint64_t pmm_total_memory_bytes(void);
uint64_t pmm_usable_memory_bytes(void);
uint64_t pmm_free_frame_count(void);
uint64_t pmm_used_frame_count(void);

void pmm_dump_mmap(void);

#endif
