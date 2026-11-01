#include "learn/learn.h"
#include "lib/kprintf.h"
#include "mm/heap.h"

#define CELL_BYTES 16          /* one character per 16 bytes of heap */
#define LINE_CELLS 48

struct alloc {
    void *ptr;
    char label;
};

struct draw_ctx {
    const struct alloc *allocs;
    int nallocs;
    char cells[LINE_CELLS + 1];
    int pos;
};

static char label_for(const struct draw_ctx *c, void *payload) {
    for (int i = 0; i < c->nallocs; i++) {
        if (c->allocs[i].ptr == payload) {
            return c->allocs[i].label;
        }
    }

    return 'x';   /* someone else's allocation */
}

static int draw_block(void *payload, size_t size, int used, void *vctx) {
    struct draw_ctx *c = (struct draw_ctx *)vctx;
    int header_cells = HEAP_HEADER_SIZE / CELL_BYTES;
    int payload_cells = (int)((size + CELL_BYTES - 1) / CELL_BYTES);
    char fill = used ? label_for(c, payload) : '.';

    for (int i = 0; i < header_cells && c->pos < LINE_CELLS; i++) {
        c->cells[c->pos++] = '#';
    }
    for (int i = 0; i < payload_cells && c->pos < LINE_CELLS; i++) {
        c->cells[c->pos++] = fill;
    }

    return c->pos >= LINE_CELLS;
}

static void draw(const char *caption, const struct alloc *allocs, int n) {
    struct draw_ctx c = { allocs, n, {0}, 0 };

    heap_walk(draw_block, &c);
    c.cells[c.pos] = '\0';
    kprintf("  %-28s %s\n", caption, c.cells);
}

void lesson_heap(void) {
    struct alloc a[6] = {{0, 0}};
    struct heap_stats st;
    int n = 0;

    learn_page("Dynamic memory: the kernel heap");
    learn_heading("Frames are too coarse; programs want 24 bytes");
    learn_para("The PMM hands out 4 KiB at a time. Kernel code needs thousands "
               "of small, variable-sized objects - a thread record, a string, "
               "a buffer - that come and go in no particular order. The heap "
               "(kmalloc/kfree) sits on top of the PMM and slices a big region "
               "into pieces on demand.");
    learn_para("heap.c's design: every block starts with a 32-byte header (a "
               "magic number, its size, and links to its neighbours). Blocks "
               "tile the region exactly. kmalloc walks the list for the first "
               "free block that is big enough (first fit) and SPLITS it if "
               "enough is left over. kfree marks a block free and COALESCES it "
               "with free neighbours so holes merge back together.");
    learn_para("Two kinds of waste: internal fragmentation (sizes are rounded "
               "up to 16 bytes, headers cost 32) and external fragmentation "
               "(free space exists but in pieces too small to use).");
    heap_get_stats(&st);
    learn_live("the heap right now");
    kprintf("  %lu bytes total: %lu used in %lu block(s), %lu free in %lu block(s)\n",
            (uint64_t)st.total_bytes, (uint64_t)st.used_bytes, (uint64_t)st.used_blocks,
            (uint64_t)st.free_bytes, (uint64_t)st.free_blocks);
    kprintf("  largest allocation that would succeed: %lu bytes\n", (uint64_t)st.largest_free);
    LEARN_NEXT();

    learn_page("Dynamic memory: the kernel heap");
    learn_live("drawing the real heap after each operation");
    kprintf("  # header   . free   a b c d ours   x other   1 char = 16 bytes\n\n");
    draw("start:", a, n);

    a[n].ptr = kmalloc(100); a[n].label = 'a'; n++;
    a[n].ptr = kmalloc(200); a[n].label = 'b'; n++;
    a[n].ptr = kmalloc(50);  a[n].label = 'c'; n++;
    draw("a=100 b=200 c=50 bytes:", a, n);

    kfree(a[1].ptr);
    a[1].ptr = 0;
    draw("kfree(b):", a, n);

    a[n].ptr = kmalloc(150); a[n].label = 'd'; n++;
    draw("d=150 (fits b's hole):", a, n);
    kprintf("\n");
    learn_para("b's old block (208 bytes) was big enough for d (160 rounded), "
               "and what was left over became a new small free block with "
               "its own header: the split. Look for the lone '.' pair after "
               "the d.");
    LEARN_NEXT_GOTO(done);

    learn_page("Dynamic memory: the kernel heap");
    learn_live("freeing and coalescing");
    kprintf("  # header   . free   a b c d ours   x other   1 char = 16 bytes\n\n");
    draw("now:", a, n);

    kfree(a[0].ptr);
    a[0].ptr = 0;
    draw("kfree(a):", a, n);

    kfree(a[3].ptr);
    a[3].ptr = 0;
    draw("kfree(d): merges both ways", a, n);

    kfree(a[2].ptr);
    a[2].ptr = 0;
    draw("kfree(c): back to one block", a, n);
    n = 0;
    kprintf("\n");
    learn_para("Freeing d found a free block on each side (a's, and the "
               "leftover from the split) and fused all three into one: "
               "coalescing. Without it the heap would shatter into unusable "
               "crumbs. heap_check() verifies exactly this invariant: no two "
               "free blocks are ever adjacent.");
    learn_try("heap dump  (every block)    kmalloc 100    heap dump    kfree <addr>");
    learn_try("exercise: make kmalloc best-fit (smallest block that fits) and "
              "compare this page.");

done:
    for (int i = 0; i < 6; i++) {
        if (a[i].ptr) {
            kfree(a[i].ptr);
        }
    }
}
