#include "mm/pmm.h"
#include "arch/log.h"
#include "layout.h"
#include "lib/kprintf.h"

#define BITMAP_BYTES (PMM_MAX_FRAMES / 8)

/* 1 = not available for allocation (reserved or already allocated). */
static uint8_t used_map[BITMAP_BYTES];

/* 1 = reserved for good. A used frame that is not reserved is "allocated". */
static uint8_t reserved_map[BITMAP_BYTES];

static const struct multiboot_tag_mmap *saved_mmap;

static uint64_t total_memory = 0;
static uint64_t usable_memory = 0;
static uint64_t free_frames = 0;
static uint64_t used_frames = 0;
static uint64_t reserved_frames = 0;

static int bit_test(const uint8_t *map, uint64_t bit) {
    return (map[bit / 8] >> (bit % 8)) & 1u;
}

static void bit_set(uint8_t *map, uint64_t bit) {
    map[bit / 8] |= (uint8_t)(1u << (bit % 8));
}

static void bit_clear(uint8_t *map, uint64_t bit) {
    map[bit / 8] &= (uint8_t)~(1u << (bit % 8));
}

/* Move one frame from free to allocated (it must currently be free). */
static void frame_take(uint64_t frame) {
    bit_set(used_map, frame);
    free_frames--;
    used_frames++;
}

/* Move one frame from allocated back to free. */
static void frame_give_back(uint64_t frame) {
    bit_clear(used_map, frame);
    free_frames++;
    used_frames--;
}

/* Mark one frame reserved, whatever state it was in. */
static void frame_reserve(uint64_t frame) {
    if (!bit_test(used_map, frame)) {
        frame_take(frame);
    }

    if (!bit_test(reserved_map, frame)) {
        bit_set(reserved_map, frame);
        reserved_frames++;
    }
}

/* Boot-time only: turn a reserved frame into a free one. */
static void frame_make_available(uint64_t frame) {
    if (bit_test(reserved_map, frame)) {
        bit_clear(reserved_map, frame);
        reserved_frames--;
        frame_give_back(frame);
    }
}

static void reserve_everything(void) {
    for (size_t i = 0; i < BITMAP_BYTES; i++) {
        used_map[i] = 0xFF;
        reserved_map[i] = 0xFF;
    }

    free_frames = 0;
    used_frames = PMM_MAX_FRAMES;
    reserved_frames = PMM_MAX_FRAMES;
}

/* Clamp [start, start + length) to the tracked range. Returns 0 if empty. */
static int clamp_range(uint64_t *start, uint64_t *end) {
    if (*start >= PMM_MAX_MEMORY_BYTES) {
        return 0;
    }

    if (*end > PMM_MAX_MEMORY_BYTES) {
        *end = PMM_MAX_MEMORY_BYTES;
    }

    return *end > *start;
}

/* Frames lying completely inside an available region become allocatable. */
static void mark_range_available(uint64_t start, uint64_t length) {
    uint64_t end = start + length;

    if (!clamp_range(&start, &end)) {
        return;
    }

    uint64_t frame_start = (start + PMM_FRAME_SIZE - 1) / PMM_FRAME_SIZE;
    uint64_t frame_end = end / PMM_FRAME_SIZE;

    for (uint64_t frame = frame_start; frame < frame_end; frame++) {
        frame_make_available(frame);
    }
}

void pmm_reserve_range(uint64_t start, uint64_t length) {
    uint64_t end = start + length;

    if (!clamp_range(&start, &end)) {
        return;
    }

    /* Round outwards: any frame touched by the range is reserved. */
    uint64_t frame_start = start / PMM_FRAME_SIZE;
    uint64_t frame_end = (end + PMM_FRAME_SIZE - 1) / PMM_FRAME_SIZE;

    for (uint64_t frame = frame_start; frame < frame_end; frame++) {
        frame_reserve(frame);
    }
}

void pmm_init(const struct multiboot_tag_mmap *mmap_tag) {
    reserve_everything();
    total_memory = 0;
    usable_memory = 0;
    saved_mmap = mmap_tag;

    if (!mmap_tag) {
        log_write("pmm: no multiboot mmap tag found\n");
        return;
    }

    const uint8_t *ptr = (const uint8_t *)mmap_tag->entries;
    const uint8_t *end = ((const uint8_t *)mmap_tag) + mmap_tag->size;

    while (ptr + sizeof(struct multiboot_mmap_entry) <= end) {
        const struct multiboot_mmap_entry *entry =
            (const struct multiboot_mmap_entry *)ptr;

        uint64_t counted_len = entry->len;

        if (entry->addr < PMM_MAX_MEMORY_BYTES) {
            if (entry->addr + entry->len > PMM_MAX_MEMORY_BYTES) {
                counted_len = PMM_MAX_MEMORY_BYTES - entry->addr;
            }

            total_memory += counted_len;

            if (entry->type == MULTIBOOT_MEMORY_AVAILABLE) {
                usable_memory += counted_len;
                mark_range_available(entry->addr, entry->len);
            }
        }

        ptr += mmap_tag->entry_size;
    }

    /* Legacy low memory: BIOS data, video memory, option ROMs. */
    pmm_reserve_range(0, 0x100000);

    /* The kernel image (code, data, bss, boot stack, page tables, bitmaps). */
    pmm_reserve_range(KERNEL_START, KERNEL_END - KERNEL_START);
}

void pmm_print_summary(void) {
    kprintf("pmm: total     %lu KiB\n", total_memory / 1024);
    kprintf("pmm: usable    %lu KiB\n", usable_memory / 1024);
    kprintf("pmm: free      %lu frames\n", free_frames);
    kprintf("pmm: reserved  %lu frames\n", reserved_frames);
    kprintf("pmm: allocated %lu frames\n", used_frames - reserved_frames);
}

void pmm_dump_mmap(void) {
    if (!saved_mmap) {
        kprintf("no memory map available\n");
        return;
    }

    kprintf("memory map (start - end, end exclusive):\n");

    const uint8_t *ptr = (const uint8_t *)saved_mmap->entries;
    const uint8_t *end = ((const uint8_t *)saved_mmap) + saved_mmap->size;

    while (ptr + sizeof(struct multiboot_mmap_entry) <= end) {
        const struct multiboot_mmap_entry *entry =
            (const struct multiboot_mmap_entry *)ptr;

        kprintf("  0x%016lx - 0x%016lx  %s\n", entry->addr,
                entry->addr + entry->len, mb2_mmap_type_name(entry->type));

        ptr += saved_mmap->entry_size;
    }

    kprintf("kernel image:\n  0x%016lx - 0x%016lx\n", KERNEL_START, KERNEL_END);
}

uint64_t pmm_alloc_contiguous(size_t count) {
    if (count == 0 || count > free_frames) {
        return 0;
    }

    uint64_t run = 0;

    for (uint64_t frame = 0; frame < PMM_MAX_FRAMES;) {
        /* Fast path: skip a whole byte of 8 busy frames at once. */
        if ((frame % 8) == 0 && used_map[frame / 8] == 0xFF) {
            run = 0;
            frame += 8;
            continue;
        }

        if (bit_test(used_map, frame)) {
            run = 0;
        } else if (++run == count) {
            uint64_t first = frame + 1 - count;

            for (uint64_t f = first; f <= frame; f++) {
                frame_take(f);
            }

            return first * PMM_FRAME_SIZE;
        }

        frame++;
    }

    return 0;
}

uint64_t pmm_alloc_frame(void) {
    return pmm_alloc_contiguous(1);
}

int pmm_free_frames(uint64_t phys_addr, size_t count) {
    if (count == 0 || (phys_addr % PMM_FRAME_SIZE) != 0) {
        return -1;
    }

    uint64_t first = phys_addr / PMM_FRAME_SIZE;

    if (first >= PMM_MAX_FRAMES || count > PMM_MAX_FRAMES - first) {
        return -1;
    }

    /* Validate the whole range before touching anything. */
    for (uint64_t frame = first; frame < first + count; frame++) {
        if (!bit_test(used_map, frame) || bit_test(reserved_map, frame)) {
            return -1;
        }
    }

    for (uint64_t frame = first; frame < first + count; frame++) {
        frame_give_back(frame);
    }

    return 0;
}

int pmm_free_frame(uint64_t phys_addr) {
    return pmm_free_frames(phys_addr, 1);
}

int pmm_is_frame_free(uint64_t phys_addr) {
    uint64_t frame = phys_addr / PMM_FRAME_SIZE;

    return frame < PMM_MAX_FRAMES && !bit_test(used_map, frame);
}

int pmm_is_frame_reserved(uint64_t phys_addr) {
    uint64_t frame = phys_addr / PMM_FRAME_SIZE;

    /* Anything beyond the tracked range is not ours to hand out. */
    return frame >= PMM_MAX_FRAMES || bit_test(reserved_map, frame);
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

uint64_t pmm_reserved_frame_count(void) {
    return reserved_frames;
}

uint64_t pmm_allocated_frame_count(void) {
    return used_frames - reserved_frames;
}
