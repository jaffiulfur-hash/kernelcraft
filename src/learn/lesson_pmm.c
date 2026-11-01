#include "learn/learn.h"
#include "arch/log.h"
#include "layout.h"
#include "lib/kprintf.h"
#include "mm/pmm.h"

/* A group of frames this lesson allocated, drawn with its own letter. */
struct held {
    uint64_t addr;
    size_t count;
    char label;
};

#define MAX_HELD 10

static char frame_char(uint64_t addr, const struct held *h, int nh) {
    for (int i = 0; i < nh; i++) {
        if (h[i].count && addr >= h[i].addr &&
            addr < h[i].addr + h[i].count * PMM_FRAME_SIZE) {
            return h[i].label;
        }
    }

    if (pmm_is_frame_free(addr)) {
        return '.';
    }

    return pmm_is_frame_reserved(addr) ? '#' : 'A';
}

static void show(const char *caption, uint64_t base, int frames,
                 const struct held *h, int nh) {
    kprintf("  %-34s ", caption);
    for (int i = 0; i < frames; i++) {
        log_write_char(frame_char(base + (uint64_t)i * PMM_FRAME_SIZE, h, nh));
    }
    kprintf("\n");
}

/* One cell of the big map summarises 128 frames (512 KiB). */
static char cell_char(uint64_t first_frame) {
    int free_n = 0, reserved_n = 0, alloc_n = 0;

    for (uint64_t f = first_frame; f < first_frame + 128; f++) {
        uint64_t addr = f * PMM_FRAME_SIZE;

        if (pmm_is_frame_free(addr)) {
            free_n++;
        } else if (pmm_is_frame_reserved(addr)) {
            reserved_n++;
        } else {
            alloc_n++;
        }
    }

    if (free_n == 128) return '.';
    if (reserved_n == 128) return '#';
    if (alloc_n == 128) return 'A';
    return '+';
}

void lesson_pmm(void) {
    struct held h[MAX_HELD] = {{0, 0, 0}};
    int nh = 0;

    learn_page("Physical memory");
    learn_heading("Who owns each 4 KiB of RAM?");
    learn_para("Before anything can allocate memory, the kernel must know what "
               "RAM exists and which parts are already taken. The firmware "
               "reports regions in the memory map ('mmap' shows it). The "
               "physical memory manager (PMM) turns that into a simple answer "
               "for every 4 KiB frame: free, reserved, or allocated.");
    learn_para("pmm.c keeps one BIT per frame, so 128 MiB costs only 4 KiB of "
               "bitmap. Two bitmaps are kept: 'used' says a frame is unavailable, "
               "'reserved' says it must never be handed out or freed (low "
               "memory, the kernel image, the boot info). Used but not "
               "reserved means allocated - that is what lets pmm_free_frame() "
               "refuse to free the kernel by mistake.");
    learn_para("Allocation is first-fit: scan the bitmap for the lowest free "
               "frame, or the lowest run of N free frames for a contiguous "
               "request (needed for DMA buffers and for the heap region).");
    learn_try("mem    mmap    alloc 4    free <that address> 4");
    LEARN_NEXT();

    learn_page("Physical memory");
    learn_live("the first 128 MiB, one character = 512 KiB (128 frames)");
    for (int row = 0; row < 4; row++) {
        kprintf("  %3d MiB  ", row * 32);
        for (int col = 0; col < 64; col++) {
            log_write_char(cell_char((uint64_t)(row * 64 + col) * 128));
        }
        kprintf("\n");
    }
    kprintf("\n  . free   # reserved   A allocated   + mixed\n\n");
    kprintf("  free %lu frames, reserved %lu, allocated %lu\n\n", pmm_free_frame_count(),
            pmm_reserved_frame_count(), pmm_allocated_frame_count());
    kprintf("  kernel image  0x%lx - 0x%lx\n\n", KERNEL_START, KERNEL_END);
    learn_para("The first 1 MiB is reserved (BIOS, VGA at 0xB8000). Then comes "
               "the kernel image, then the 1 MiB heap taken from the PMM at "
               "boot (the 'A' run). The '#' blocks at the very top are memory "
               "the firmware keeps for itself. Everything else is free.");
    LEARN_NEXT();

    /* Find where the next frames will come from, to centre the window there. */
    uint64_t probe = pmm_alloc_frame();
    if (probe == 0) {
        kprintf("out of memory\n");
        return;
    }
    pmm_free_frame(probe);
    uint64_t base = probe - 4 * PMM_FRAME_SIZE;
    const int window = 32;

    learn_page("Physical memory");
    learn_live("allocating and freeing real frames (first fit)");
    kprintf("  window starts at frame address 0x%lx, one character per frame\n", base);
    kprintf("  . free  # reserved  A allocated elsewhere  a b c ... ours\n\n");
    show("before:", base, window, h, nh);

    for (char label = 'a'; label <= 'c'; label++) {
        h[nh].addr = pmm_alloc_frame();
        h[nh].count = 1;
        h[nh].label = label;
        nh++;
    }
    show("alloc a, b, c (lowest free each):", base, window, h, nh);

    pmm_free_frame(h[1].addr);
    h[1].count = 0;
    show("free b (leaves a hole):", base, window, h, nh);

    h[nh].addr = pmm_alloc_frame();
    h[nh].count = 1;
    h[nh].label = 'd';
    nh++;
    show("alloc d (first fit refills hole):", base, window, h, nh);
    kprintf("\n");
    learn_para("Lowest free frame wins, so d lands exactly in the hole b left. "
               "That is first-fit: simple and fast to reason about, but it "
               "tends to fragment memory as the system runs for a long time.");
    LEARN_NEXT_GOTO(done);

    learn_page("Physical memory");
    learn_live("contiguous allocation and fragmentation");
    kprintf("  . free  # reserved  A allocated elsewhere  letters: ours\n\n");

    /* Release everything and build a checkerboard of single frames. */
    for (int i = 0; i < nh; i++) {
        if (h[i].count) {
            pmm_free_frame(h[i].addr);
        }
    }
    nh = 0;

    uint64_t first = pmm_alloc_contiguous(8);
    if (first == 0) {
        goto done;
    }
    for (int i = 0; i < 8; i++) {
        h[nh].addr = first + (uint64_t)i * PMM_FRAME_SIZE;
        h[nh].count = 1;
        h[nh].label = (char)('1' + i);
        nh++;
    }
    show("8 frames, one alloc call:", base, window, h, nh);

    for (int i = 1; i < 8; i += 2) {
        pmm_free_frame(h[i].addr);
        h[i].count = 0;
    }
    show("free every other one:", base, window, h, nh);

    uint64_t run = pmm_alloc_contiguous(3);
    h[nh].addr = run;
    h[nh].count = run ? 3 : 0;
    h[nh].label = 'X';
    nh++;
    show("alloc 3 CONTIGUOUS frames (XXX):", base, window, h, nh);
    kprintf("\n");
    learn_para("Four single frames are free, yet a request for 3 in a row cannot "
               "use them: the allocator skipped past the checkerboard. That is "
               "external fragmentation - plenty of free memory, none of it in "
               "one piece. It is why kernels keep separate pools for "
               "contiguous needs, and why paging (next lesson) matters: with "
               "virtual memory, scattered frames can look contiguous.");
    learn_try("change pmm_alloc_contiguous() to search from the top of memory "
              "downward and watch this page change.");

done:
    for (int i = 0; i < nh; i++) {
        if (h[i].count) {
            pmm_free_frames(h[i].addr, h[i].count);
        }
    }
}
