#ifndef MM_PMM_H
#define MM_PMM_H

#include <stddef.h>
#include <stdint.h>
#include "boot/multiboot2.h"

#define PMM_FRAME_SIZE 4096
#define PMM_MAX_MEMORY_BYTES (128ull * 1024ull * 1024ull)
#define PMM_MAX_FRAMES (PMM_MAX_MEMORY_BYTES / PMM_FRAME_SIZE)

/*
 * Physical memory manager: one bit per 4 KiB frame.
 *
 * Every frame is in one of three states:
 *   free      - available to pmm_alloc_*
 *   reserved  - never handed out and never freeable: low memory, the kernel
 *               image, the boot info block, and anything the firmware map
 *               does not list as available
 *   allocated - handed out by pmm_alloc_*, can be returned with pmm_free_*
 *
 * Only the first PMM_MAX_MEMORY_BYTES of RAM are tracked. Allocation failure
 * is reported as address 0, which is safe because frame 0 is always reserved.
 */

/* Build the free/reserved map from the Multiboot2 memory map. */
void pmm_init(const struct multiboot_tag_mmap *mmap_tag);

/* Permanently reserve [start, start + length), rounded outwards to frames. */
void pmm_reserve_range(uint64_t start, uint64_t length);

/* Allocate one frame / `count` physically contiguous frames. 0 on failure. */
uint64_t pmm_alloc_frame(void);
uint64_t pmm_alloc_contiguous(size_t count);

/*
 * Return frames to the allocator. Returns 0 on success, -1 if the address is
 * unaligned, out of range, reserved, or (for multi-frame frees) not entirely
 * allocated. A failed free changes nothing.
 */
int pmm_free_frame(uint64_t phys_addr);
int pmm_free_frames(uint64_t phys_addr, size_t count);

int pmm_is_frame_free(uint64_t phys_addr);
int pmm_is_frame_reserved(uint64_t phys_addr);

uint64_t pmm_total_memory_bytes(void);
uint64_t pmm_usable_memory_bytes(void);
uint64_t pmm_free_frame_count(void);
uint64_t pmm_used_frame_count(void);       /* reserved + allocated */
uint64_t pmm_reserved_frame_count(void);
uint64_t pmm_allocated_frame_count(void);

void pmm_print_summary(void);
void pmm_dump_mmap(void);

#endif
