#include "mm/pmm.h"
#include "arch/log.h"

static uint8_t pmm_bitmap[PMM_MAX_FRAMES / 8];
static uint64_t total_memory = 0;
static uint64_t usable_memory = 0;
static uint64_t free_frames = 0;
static uint64_t used_frames = 0;

static void bitmap_set(size_t bit) {
    pmm_bitmap[bit / 8] |= (uint8_t)(1u << (bit % 8));
}

static void bitmap_clear(size_t bit) {
    pmm_bitmap[bit / 8] &= (uint8_t)~(1u << (bit % 8));
}

static int bitmap_test(size_t bit) {
    return (pmm_bitmap[bit / 8] >> (bit % 8)) & 1u;
}

static void reserve_all_frames(void) {
    for (size_t i = 0; i < sizeof(pmm_bitmap); i++) {
        pmm_bitmap[i] = 0xFF;
    }

    free_frames = 0;
    used_frames = PMM_MAX_FRAMES;
}

static void mark_range_free(uint64_t start, uint64_t length) {
    uint64_t end = start + length;

    if (start >= PMM_MAX_MEMORY_BYTES) {
        return;
    }

    if (end > PMM_MAX_MEMORY_BYTES) {
        end = PMM_MAX_MEMORY_BYTES;
    }

    uint64_t frame_start = (start + PMM_FRAME_SIZE - 1) / PMM_FRAME_SIZE;
    uint64_t frame_end = end / PMM_FRAME_SIZE;

    for (uint64_t frame = frame_start; frame < frame_end; frame++) {
        if (bitmap_test((size_t)frame)) {
            bitmap_clear((size_t)frame);
            free_frames++;
            used_frames--;
        }
    }
}

static void mark_range_used(uint64_t start, uint64_t length) {
    uint64_t end = start + length;

    if (start >= PMM_MAX_MEMORY_BYTES) {
        return;
    }

    if (end > PMM_MAX_MEMORY_BYTES) {
        end = PMM_MAX_MEMORY_BYTES;
    }

    uint64_t frame_start = start / PMM_FRAME_SIZE;
    uint64_t frame_end = (end + PMM_FRAME_SIZE - 1) / PMM_FRAME_SIZE;

    for (uint64_t frame = frame_start; frame < frame_end; frame++) {
        if (!bitmap_test((size_t)frame)) {
            bitmap_set((size_t)frame);
            free_frames--;
            used_frames++;
        }
    }
}

void pmm_dump_mmap(void) {
    log_write("\nmemory map:\n");
}

void pmm_init(const struct multiboot_tag_mmap *mmap_tag) {
    reserve_all_frames();
    total_memory = 0;
    usable_memory = 0;

    if (!mmap_tag) {
        log_write("pmm: no multiboot mmap tag found\n");
        return;
    }

    uint8_t *ptr = (uint8_t *)mmap_tag->entries;
    uint8_t *end = ((uint8_t *)mmap_tag) + mmap_tag->size;

    while (ptr < end) {
        const struct multiboot_mmap_entry *entry =
            (const struct multiboot_mmap_entry *)ptr;

        uint64_t region_end = entry->addr + entry->len;
        uint64_t counted_len = entry->len;

        if (entry->addr < PMM_MAX_MEMORY_BYTES) {
            if (region_end > PMM_MAX_MEMORY_BYTES) {
                counted_len = PMM_MAX_MEMORY_BYTES - entry->addr;
            }

            total_memory += counted_len;

            if (entry->type == MULTIBOOT_MEMORY_AVAILABLE) {
                usable_memory += counted_len;
                mark_range_free(entry->addr, entry->len);
            }
        }

        ptr += mmap_tag->entry_size;
    }

    /* Keep low memory and kernel area reserved. */
    mark_range_used(0x00000000, 0x00100000);

    /* Keep the first 4 MiB reserved for now to avoid stepping on kernel/boot data. */
    mark_range_used(0x00100000, 0x00300000);

    log_write("pmm: initialized\n");
    log_write("pmm: total bytes=");
    log_write_hex_u64(total_memory);
    log_write("\npmm: usable bytes=");
    log_write_hex_u64(usable_memory);
    log_write("\npmm: free frames=");
    log_write_hex_u64(free_frames);
    log_write("\npmm: used frames=");
    log_write_hex_u64(used_frames);
    log_write("\n");
}

uint64_t pmm_alloc_frame(void) {
    for (uint64_t frame = 0; frame < PMM_MAX_FRAMES; frame++) {
        if (!bitmap_test((size_t)frame)) {
            bitmap_set((size_t)frame);
            free_frames--;
            used_frames++;
            return frame * PMM_FRAME_SIZE;
        }
    }

    return 0;
}

void pmm_free_frame(uint64_t phys_addr) {
    uint64_t frame = phys_addr / PMM_FRAME_SIZE;

    if (frame >= PMM_MAX_FRAMES) {
        return;
    }

    if (bitmap_test((size_t)frame)) {
        bitmap_clear((size_t)frame);
        free_frames++;
        used_frames--;
    }
}

uint64_t pmm_total_memory_bytes(void) {
    return total_memory;
}

uint64_t pmm_usable_memory_bytes(void) {
    return usable_memory;
}

uint64_t pmm_free_frame_count(void) {
    return free_frames;
}

uint64_t pmm_used_frame_count(void) {
    return used_frames;
}
