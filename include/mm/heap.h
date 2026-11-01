#ifndef MM_HEAP_H
#define MM_HEAP_H

#include <stddef.h>
#include <stdint.h>

/*
 * Kernel heap: a first-fit allocator over one physically contiguous region
 * taken from the PMM at boot. Memory is identity mapped, so the region is
 * usable directly. The heap is fixed-size; growing it will need paging.
 *
 * Every allocation is preceded by a 32-byte header and rounded up to a
 * multiple of 16 bytes, so returned pointers are 16-byte aligned.
 *
 * Thread safety: kmalloc, kfree and the stats/check helpers run with
 * interrupts disabled, which is a complete lock on one CPU because the timer
 * interrupt is what drives preemption. heap_walk() is not locked since it
 * calls back into arbitrary code.
 */

#define HEAP_FRAMES 256                       /* 1 MiB */
#define HEAP_ALIGN  16
#define HEAP_HEADER_SIZE 32                   /* bytes before every payload */

struct heap_stats {
    size_t total_bytes;      /* size of the whole region, headers included */
    size_t used_bytes;       /* payload bytes in allocated blocks */
    size_t free_bytes;       /* payload bytes in free blocks */
    size_t used_blocks;
    size_t free_blocks;
    size_t largest_free;     /* biggest single allocation that would succeed */
    size_t header_bytes;     /* overhead: blocks * sizeof(header) */
};

void heap_init(void);

/* Returns NULL on failure or when size == 0. */
void *kmalloc(size_t size);
void *kcalloc(size_t count, size_t size);

/* kfree(NULL) is a no-op. Freeing a bad pointer or double freeing panics. */
void kfree(void *ptr);

void heap_get_stats(struct heap_stats *out);

/* 1 if ptr is exactly the pointer returned by a live kmalloc, else 0. */
int heap_is_live_ptr(const void *ptr);

/* Walk and validate every block header. Returns 0 if consistent, -1 if not. */
int heap_check(void);

void heap_dump(void);

/*
 * Visit every block in address order: fn(payload, size, used, ctx). Return
 * nonzero from fn to stop early. Used by the heap lesson to draw the heap.
 */
typedef int (*heap_walk_fn)(void *payload, size_t size, int used, void *ctx);
void heap_walk(heap_walk_fn fn, void *ctx);

#endif
