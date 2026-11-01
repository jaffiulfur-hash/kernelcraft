#include "mm/heap.h"
#include "mm/pmm.h"
#include "lib/kprintf.h"
#include "lib/string.h"
#include "panic.h"

#define HEAP_BYTES ((size_t)HEAP_FRAMES * PMM_FRAME_SIZE)

#define MAGIC_FREE 0xF4EEB10Cu
#define MAGIC_USED 0xA110CA7Eu

/*
 * The heap is one address-ordered, doubly linked list of blocks that exactly
 * tile the region: each block's payload ends where the next header begins.
 * Allocation splits a free block; freeing merges it with free neighbours.
 */
struct heap_block {
    uint32_t magic;
    uint32_t pad;
    size_t size;                 /* payload bytes, multiple of HEAP_ALIGN */
    struct heap_block *prev;
    struct heap_block *next;
};

_Static_assert(sizeof(struct heap_block) % HEAP_ALIGN == 0,
               "heap header must keep payloads aligned");

#define HEADER_SIZE (sizeof(struct heap_block))

static uint8_t *heap_start;
static uint8_t *heap_end;
static struct heap_block *first_block;

static size_t align_up(size_t n) {
    return (n + (HEAP_ALIGN - 1)) & ~(size_t)(HEAP_ALIGN - 1);
}

static void *block_payload(struct heap_block *b) {
    return (uint8_t *)b + HEADER_SIZE;
}

static uint8_t *block_end(const struct heap_block *b) {
    return (uint8_t *)b + HEADER_SIZE + b->size;
}

void heap_init(void) {
    uint64_t phys = pmm_alloc_contiguous(HEAP_FRAMES);

    if (phys == 0) {
        PANIC("heap: cannot reserve contiguous frames for the heap");
    }

    heap_start = (uint8_t *)(uintptr_t)phys;
    heap_end = heap_start + HEAP_BYTES;

    first_block = (struct heap_block *)heap_start;
    first_block->magic = MAGIC_FREE;
    first_block->pad = 0;
    first_block->size = HEAP_BYTES - HEADER_SIZE;
    first_block->prev = 0;
    first_block->next = 0;

    kprintf("heap: %lu KiB at %p\n", (uint64_t)(HEAP_BYTES / 1024), heap_start);
}

/* Cut `b` down to `size` bytes if the tail is big enough to be a block. */
static void split_block(struct heap_block *b, size_t size) {
    if (b->size < size + HEADER_SIZE + HEAP_ALIGN) {
        return;
    }

    struct heap_block *tail = (struct heap_block *)((uint8_t *)b + HEADER_SIZE + size);

    tail->magic = MAGIC_FREE;
    tail->pad = 0;
    tail->size = b->size - size - HEADER_SIZE;
    tail->prev = b;
    tail->next = b->next;

    if (b->next) {
        b->next->prev = tail;
    }

    b->next = tail;
    b->size = size;
}

void *kmalloc(size_t size) {
    if (!first_block || size == 0 || size > HEAP_BYTES) {
        return 0;
    }

    size = align_up(size);

    for (struct heap_block *b = first_block; b; b = b->next) {
        if (b->magic != MAGIC_FREE && b->magic != MAGIC_USED) {
            PANIC("kmalloc: heap corruption (bad block magic)");
        }

        if (b->magic == MAGIC_FREE && b->size >= size) {
            split_block(b, size);
            b->magic = MAGIC_USED;
            return block_payload(b);
        }
    }

    return 0;
}

void *kcalloc(size_t count, size_t size) {
    if (count != 0 && size > HEAP_BYTES / count) {
        return 0;   /* count * size would overflow or exceed the heap */
    }

    size_t total = count * size;
    void *p = kmalloc(total);

    if (p) {
        memset(p, 0, total);
    }

    return p;
}

/* Find the block whose payload starts exactly at ptr (walks the list). */
static struct heap_block *find_block(const void *ptr) {
    for (struct heap_block *b = first_block; b; b = b->next) {
        if (block_payload(b) == ptr) {
            return b;
        }
    }

    return 0;
}

int heap_is_live_ptr(const void *ptr) {
    struct heap_block *b = find_block(ptr);

    return b && b->magic == MAGIC_USED;
}

void kfree(void *ptr) {
    if (!ptr) {
        return;
    }

    struct heap_block *b = find_block(ptr);

    if (!b) {
        PANIC("kfree: pointer was not returned by kmalloc");
    }

    if (b->magic == MAGIC_FREE) {
        PANIC("kfree: double free");
    }

    b->magic = MAGIC_FREE;

    /* Merge with the following block if it is free. */
    if (b->next && b->next->magic == MAGIC_FREE) {
        b->size += HEADER_SIZE + b->next->size;
        b->next = b->next->next;
        if (b->next) {
            b->next->prev = b;
        }
    }

    /* Merge into the preceding block if it is free. */
    if (b->prev && b->prev->magic == MAGIC_FREE) {
        struct heap_block *p = b->prev;

        p->size += HEADER_SIZE + b->size;
        p->next = b->next;
        if (p->next) {
            p->next->prev = p;
        }
    }
}

void heap_get_stats(struct heap_stats *out) {
    memset(out, 0, sizeof(*out));
    out->total_bytes = HEAP_BYTES;

    for (struct heap_block *b = first_block; b; b = b->next) {
        out->header_bytes += HEADER_SIZE;

        if (b->magic == MAGIC_USED) {
            out->used_blocks++;
            out->used_bytes += b->size;
        } else {
            out->free_blocks++;
            out->free_bytes += b->size;
            if (b->size > out->largest_free) {
                out->largest_free = b->size;
            }
        }
    }
}

int heap_check(void) {
    if (!first_block) {
        return -1;
    }

    if ((uint8_t *)first_block != heap_start || first_block->prev) {
        return -1;
    }

    size_t covered = 0;

    for (struct heap_block *b = first_block; b; b = b->next) {
        if ((uint8_t *)b < heap_start || block_end(b) > heap_end) {
            return -1;
        }

        if (b->magic != MAGIC_FREE && b->magic != MAGIC_USED) {
            return -1;
        }

        if (b->size % HEAP_ALIGN != 0) {
            return -1;
        }

        if (b->next) {
            if (b->next->prev != b || (uint8_t *)b->next != block_end(b)) {
                return -1;
            }
            /* Two free neighbours should have been merged by kfree. */
            if (b->magic == MAGIC_FREE && b->next->magic == MAGIC_FREE) {
                return -1;
            }
        } else if (block_end(b) != heap_end) {
            return -1;
        }

        covered += HEADER_SIZE + b->size;
    }

    return covered == HEAP_BYTES ? 0 : -1;
}

void heap_dump(void) {
    kprintf("heap blocks (address, payload bytes, state):\n");

    for (struct heap_block *b = first_block; b; b = b->next) {
        kprintf("  %p  %8lu  %s\n", block_payload(b), (uint64_t)b->size,
                b->magic == MAGIC_USED ? "used" : "free");
    }
}
