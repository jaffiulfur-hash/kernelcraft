#include "selftest.h"
#include "arch/keyboard.h"
#include "arch/rtc.h"
#include "boot/multiboot2.h"
#include "learn/learn.h"
#include "layout.h"
#include "lib/kprintf.h"
#include "lib/string.h"
#include "mm/heap.h"
#include "mm/pmm.h"

static uint32_t boot_info_addr;
static int check_failures;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            kprintf("    check failed: %s (selftest.c:%d)\n", #cond, __LINE__); \
            check_failures++;                                                \
        }                                                                    \
    } while (0)

static void check_str(const char *got, const char *want, int line) {
    if (strcmp(got, want) != 0) {
        kprintf("    format mismatch (selftest.c:%d): got \"%s\", want \"%s\"\n",
                line, got, want);
        check_failures++;
    }
}

/* Format into `buf` and compare against `want`. */
#define CHECK_FMT(want, ...)                                    \
    do {                                                        \
        ksnprintf(buf, sizeof(buf), __VA_ARGS__);               \
        check_str(buf, (want), __LINE__);                       \
    } while (0)

void selftest_init(uint32_t mb_info_addr) {
    boot_info_addr = mb_info_addr;
}

/* ---- lib/string ------------------------------------------------------- */

static void test_string(void) {
    uint8_t a[8], b[8], buf[8];
    uint64_t v = 0;

    CHECK(strlen("") == 0);
    CHECK(strlen("kernel") == 6);

    CHECK(strcmp("abc", "abc") == 0);
    CHECK(strcmp("abc", "abd") < 0);
    CHECK(strcmp("b", "a") > 0);
    CHECK(strcmp("ab", "abc") < 0);
    CHECK(strncmp("abcd", "abzz", 2) == 0);
    CHECK(strncmp("abcd", "abzz", 3) < 0);
    CHECK(strncmp("a", "b", 0) == 0);

    CHECK(strcasecmp("Page Fault", "page fault") == 0);
    CHECK(strcasecmp("abc", "abd") < 0);
    CHECK(strncasecmp("HELLO", "help", 3) == 0);
    CHECK(strncasecmp("HELLO", "help", 4) != 0);

    memset(a, 0xAB, sizeof(a));
    CHECK(a[0] == 0xAB && a[7] == 0xAB);

    for (int i = 0; i < 8; i++) {
        b[i] = (uint8_t)i;
    }
    memcpy(a, b, sizeof(a));
    CHECK(memcmp(a, b, sizeof(a)) == 0);
    a[5] ^= 0xFF;
    CHECK(memcmp(a, b, sizeof(a)) != 0);

    /* Overlapping copies in both directions. */
    for (int i = 0; i < 8; i++) {
        buf[i] = (uint8_t)i;
    }
    memmove(buf + 2, buf, 6);
    CHECK(buf[0] == 0 && buf[1] == 1 && buf[2] == 0 && buf[7] == 5);

    for (int i = 0; i < 8; i++) {
        buf[i] = (uint8_t)i;
    }
    memmove(buf, buf + 2, 6);
    CHECK(buf[0] == 2 && buf[5] == 7 && buf[6] == 6 && buf[7] == 7);

    CHECK(str_to_u64("0", &v) == 0 && v == 0);
    CHECK(str_to_u64("123", &v) == 0 && v == 123);
    CHECK(str_to_u64("0x7B", &v) == 0 && v == 123);
    CHECK(str_to_u64("0Xff", &v) == 0 && v == 255);
    CHECK(str_to_u64("18446744073709551615", &v) == 0 && v == UINT64_MAX);
    CHECK(str_to_u64("0xFFFFFFFFFFFFFFFF", &v) == 0 && v == UINT64_MAX);

    v = 77;
    CHECK(str_to_u64("", &v) == -1);
    CHECK(str_to_u64("12a", &v) == -1);
    CHECK(str_to_u64("0x", &v) == -1);
    CHECK(str_to_u64("0xZ", &v) == -1);
    CHECK(str_to_u64("-5", &v) == -1);
    CHECK(str_to_u64("18446744073709551616", &v) == -1);
    CHECK(str_to_u64("0x10000000000000000", &v) == -1);
    CHECK(v == 77);   /* untouched on failure */
}

/* ---- lib/kprintf ------------------------------------------------------ */

static void test_kprintf(void) {
    char buf[64];
    char small[4];
    const char *volatile null_str = 0;   /* volatile: keep the NULL opaque to GCC */

    CHECK_FMT("-42", "%d", -42);
    CHECK_FMT("0", "%d", 0);
    CHECK_FMT("4000000000", "%u", 4000000000u);
    CHECK_FMT("ff", "%x", 255);
    CHECK_FMT("FF", "%X", 255);
    CHECK_FMT("000000ff", "%08x", 255);
    CHECK_FMT("   42|", "%5d|", 42);
    CHECK_FMT("42   |", "%-5d|", 42);
    CHECK_FMT("-0042", "%05d", -42);
    CHECK_FMT("-2147483648", "%d", (int)(-2147483647 - 1));
    CHECK_FMT("1099511627776", "%lu", 1ul << 40);
    CHECK_FMT("-5", "%ld", -5l);
    CHECK_FMT("-9223372036854775808", "%ld", (long)(-9223372036854775807l - 1));
    CHECK_FMT("deadbeefcafe", "%lx", 0xDEADBEEFCAFEul);
    CHECK_FMT("0x0000000000001234", "%p", (void *)0x1234);

    CHECK_FMT("hi", "%s", "hi");
    CHECK_FMT("    hi|", "%6s|", "hi");
    CHECK_FMT("hi    |", "%-6s|", "hi");
    CHECK_FMT("(null)", "%s", null_str);
    CHECK_FMT("A", "%c", 'A');
    CHECK_FMT("  x|", "%3c|", 'x');
    CHECK_FMT("100%", "100%%");
    CHECK_FMT("a=1 b=ff", "a=%d b=%x", 1, 255);

    /* Truncation: output is cut but the full length is reported. */
    int full = ksnprintf(small, sizeof(small), "abcdef");
    CHECK(full == 6);
    CHECK(strcmp(small, "abc") == 0);
    CHECK(ksnprintf(0, 0, "abc") == 3);
}

/* ---- mm/pmm ----------------------------------------------------------- */

static int range_all_reserved(uint64_t start, uint64_t length) {
    uint64_t first = start & ~(uint64_t)(PMM_FRAME_SIZE - 1);

    for (uint64_t addr = first; addr < start + length; addr += PMM_FRAME_SIZE) {
        if (!pmm_is_frame_reserved(addr)) {
            return 0;
        }
    }

    return 1;
}

static void test_pmm_reservations(void) {
    CHECK(range_all_reserved(0, 0x100000));                       /* low memory */
    CHECK(range_all_reserved(0xB8000, 4000));                     /* VGA text */
    CHECK(range_all_reserved(KERNEL_START, KERNEL_END - KERNEL_START));
    CHECK(range_all_reserved(boot_info_addr, mb2_info_size(boot_info_addr)));

    CHECK(pmm_is_frame_reserved(PMM_MAX_MEMORY_BYTES));           /* untracked */

    /* The bookkeeping must agree with the bitmap, frame by frame. */
    uint64_t free_seen = 0;
    for (uint64_t f = 0; f < PMM_MAX_FRAMES; f++) {
        if (pmm_is_frame_free(f * PMM_FRAME_SIZE)) {
            free_seen++;
        }
    }
    CHECK(free_seen == pmm_free_frame_count());
    CHECK(pmm_free_frame_count() + pmm_used_frame_count() == PMM_MAX_FRAMES);
    CHECK(pmm_reserved_frame_count() + pmm_allocated_frame_count() ==
          pmm_used_frame_count());
}

static void test_pmm_single_frames(void) {
    uint64_t free0 = pmm_free_frame_count();
    uint64_t alloc0 = pmm_allocated_frame_count();

    uint64_t a = pmm_alloc_frame();
    CHECK(a != 0);
    CHECK(a % PMM_FRAME_SIZE == 0);
    CHECK(!(a >= KERNEL_START && a < KERNEL_END));
    CHECK(!pmm_is_frame_free(a));
    CHECK(!pmm_is_frame_reserved(a));
    CHECK(pmm_free_frame_count() == free0 - 1);
    CHECK(pmm_allocated_frame_count() == alloc0 + 1);

    /* A second allocation must not hand out the same frame. */
    uint64_t b = pmm_alloc_frame();
    CHECK(b != 0 && b != a);

    CHECK(pmm_free_frame(a) == 0);
    CHECK(pmm_is_frame_free(a));
    CHECK(pmm_free_frame(b) == 0);
    CHECK(pmm_free_frame_count() == free0);
    CHECK(pmm_allocated_frame_count() == alloc0);

    /* Bad frees must be rejected and change nothing. */
    CHECK(pmm_free_frame(a) == -1);                 /* double free */
    CHECK(pmm_free_frame(a + 1) == -1);             /* unaligned */
    CHECK(pmm_free_frame(0) == -1);                 /* reserved */
    CHECK(pmm_free_frame(KERNEL_START) == -1);      /* kernel image */
    CHECK(pmm_free_frame(PMM_MAX_MEMORY_BYTES) == -1);
    CHECK(pmm_free_frames(a, 0) == -1);
    CHECK(pmm_free_frame_count() == free0);
    CHECK(pmm_allocated_frame_count() == alloc0);
}

static void test_pmm_contiguous(void) {
    uint64_t free0 = pmm_free_frame_count();
    uint64_t alloc0 = pmm_allocated_frame_count();

    CHECK(pmm_alloc_contiguous(0) == 0);
    CHECK(pmm_alloc_contiguous(PMM_MAX_FRAMES + 1) == 0);
    CHECK(pmm_alloc_contiguous(free0 + 1) == 0);
    CHECK(pmm_free_frame_count() == free0);

    uint64_t run = pmm_alloc_contiguous(8);
    CHECK(run != 0);
    CHECK(run % PMM_FRAME_SIZE == 0);
    CHECK(pmm_allocated_frame_count() == alloc0 + 8);

    int all_used = 1;
    for (int i = 0; i < 8; i++) {
        if (pmm_is_frame_free(run + (uint64_t)i * PMM_FRAME_SIZE)) {
            all_used = 0;
        }
    }
    CHECK(all_used);

    /* Punch a hole: freeing the whole run must now fail atomically. */
    CHECK(pmm_free_frame(run + 3 * PMM_FRAME_SIZE) == 0);
    CHECK(pmm_free_frames(run, 8) == -1);
    CHECK(pmm_allocated_frame_count() == alloc0 + 7);

    CHECK(pmm_free_frames(run, 3) == 0);
    CHECK(pmm_free_frames(run + 4 * PMM_FRAME_SIZE, 4) == 0);
    CHECK(pmm_free_frame_count() == free0);
    CHECK(pmm_allocated_frame_count() == alloc0);
}

/* ---- mm/heap ---------------------------------------------------------- */

static int is_filled(const void *p, uint8_t value, size_t n) {
    const uint8_t *bytes = (const uint8_t *)p;

    for (size_t i = 0; i < n; i++) {
        if (bytes[i] != value) {
            return 0;
        }
    }

    return 1;
}

static int stats_equal(const struct heap_stats *x, const struct heap_stats *y) {
    return x->used_bytes == y->used_bytes && x->free_bytes == y->free_bytes &&
           x->used_blocks == y->used_blocks && x->free_blocks == y->free_blocks;
}

static void test_heap_basic(void) {
    struct heap_stats before, after;

    heap_get_stats(&before);
    CHECK(heap_check() == 0);
    CHECK(before.total_bytes == (size_t)HEAP_FRAMES * PMM_FRAME_SIZE);
    CHECK(before.used_bytes + before.free_bytes + before.header_bytes ==
          before.total_bytes);

    CHECK(kmalloc(0) == 0);
    CHECK(kmalloc(before.total_bytes) == 0);        /* header does not fit */
    CHECK(kmalloc((size_t)-1) == 0);
    CHECK(heap_check() == 0);

    void *a = kmalloc(1);
    void *b = kmalloc(100);
    void *c = kmalloc(4096);

    CHECK(a && b && c);
    CHECK(a != b && b != c && a != c);
    CHECK(((uintptr_t)a % HEAP_ALIGN) == 0);
    CHECK(((uintptr_t)b % HEAP_ALIGN) == 0);
    CHECK(((uintptr_t)c % HEAP_ALIGN) == 0);
    CHECK(heap_is_live_ptr(a) && heap_is_live_ptr(b) && heap_is_live_ptr(c));
    CHECK(!heap_is_live_ptr((uint8_t *)b + 1));
    CHECK(!heap_is_live_ptr(0));

    /* Allocations must not overlap: write distinct patterns, read them back. */
    memset(a, 0x11, 1);
    memset(b, 0x22, 100);
    memset(c, 0x33, 4096);
    CHECK(is_filled(a, 0x11, 1));
    CHECK(is_filled(b, 0x22, 100));
    CHECK(is_filled(c, 0x33, 4096));
    CHECK(heap_check() == 0);

    /* First fit reuses a hole left by a freed block. */
    kfree(b);
    CHECK(!heap_is_live_ptr(b));
    CHECK(heap_check() == 0);
    void *d = kmalloc(64);
    CHECK(d != 0);
    if (before.used_blocks == 0 && before.free_blocks == 1) {
        /* Pristine heap: the hole left by b is the first fit. When the user
         * has already fragmented the heap an earlier hole may win instead. */
        CHECK(d == b);
    }
    kfree(d);

    /* Freeing everything must coalesce back to the starting layout. */
    kfree(a);
    kfree(c);
    CHECK(heap_check() == 0);
    heap_get_stats(&after);
    CHECK(stats_equal(&before, &after));

    kfree(0);   /* must be a no-op */
    CHECK(heap_check() == 0);
}

static void test_heap_calloc(void) {
    struct heap_stats before, after;

    heap_get_stats(&before);

    /* Dirty a block, free it, then expect calloc to hand back zeros. */
    uint8_t *dirty = (uint8_t *)kmalloc(128);
    CHECK(dirty != 0);
    memset(dirty, 0xFF, 128);
    kfree(dirty);

    uint8_t *z = (uint8_t *)kcalloc(16, 8);
    CHECK(z != 0);
    CHECK(is_filled(z, 0, 128));
    kfree(z);

    CHECK(kcalloc(0, 8) == 0);
    CHECK(kcalloc((size_t)-1, 2) == 0);             /* multiplication overflow */

    CHECK(heap_check() == 0);
    heap_get_stats(&after);
    CHECK(stats_equal(&before, &after));
}

static void test_heap_fragmentation(void) {
    struct heap_stats before, after;
    void *p[8];

    heap_get_stats(&before);

    for (int i = 0; i < 8; i++) {
        p[i] = kmalloc(48);
        CHECK(p[i] != 0);
    }

    /* Free every other block: creates holes that cannot merge yet. */
    for (int i = 1; i < 8; i += 2) {
        kfree(p[i]);
        CHECK(heap_check() == 0);
    }

    /* Free the rest in an order that exercises merge-next and merge-prev. */
    kfree(p[2]);
    kfree(p[0]);
    kfree(p[6]);
    kfree(p[4]);
    CHECK(heap_check() == 0);

    heap_get_stats(&after);
    CHECK(stats_equal(&before, &after));
}

static void test_heap_exhaustion(void) {
    struct heap_stats before, after;
    void *chunks[32];
    int count = 0;

    heap_get_stats(&before);

    /* 64 KiB chunks: the 1 MiB heap cannot satisfy 32 of them. */
    while (count < 32) {
        chunks[count] = kmalloc(64 * 1024);
        if (!chunks[count]) {
            break;
        }
        count++;
    }

    if (before.largest_free >= 64 * 1024) {
        CHECK(count > 0);
    }
    CHECK(count < 32);
    CHECK(heap_check() == 0);

    for (int i = 0; i < count; i++) {
        kfree(chunks[i]);
    }

    CHECK(heap_check() == 0);
    heap_get_stats(&after);
    CHECK(stats_equal(&before, &after));
}


/* ---- keyboard ring, RTC, glossary, lessons ------------------------------ */

static void test_keyboard_ring(void) {
    char c;
    int kept = 0;

    while (keyboard_try_read(&c)) {
    }
    CHECK(!keyboard_try_read(&c));

    keyboard_push_char('a');
    keyboard_push_char('b');
    CHECK(keyboard_try_read(&c) && c == 'a');
    CHECK(keyboard_try_read(&c) && c == 'b');
    CHECK(!keyboard_try_read(&c));

    /* Overfilling drops the extra keystrokes instead of corrupting the ring. */
    for (int i = 0; i < 100; i++) {
        keyboard_push_char((char)('A' + i % 26));
    }
    CHECK(keyboard_try_read(&c) && c == 'A');
    kept = 1;
    while (keyboard_try_read(&c)) {
        kept++;
    }
    CHECK(kept == 63);          /* a 64-slot ring holds 63 */

    /* keyboard_getc returns an already-queued character without sleeping. */
    keyboard_push_char('z');
    CHECK(keyboard_getc() == 'z');
}

static void test_rtc(void) {
    struct rtc_time t;

    rtc_read(&t);
    CHECK(t.year >= 2000 && t.year < 2100);
    CHECK(t.month >= 1 && t.month <= 12);
    CHECK(t.day >= 1 && t.day <= 31);
    CHECK(t.hour < 24 && t.minute < 60 && t.second < 60);
}

static void test_glossary(void) {
    CHECK(glossary_term_count() > 30);
    CHECK(glossary_lookup("PAGE FAULT") != 0);
    CHECK(glossary_lookup("gdt") != 0);
    CHECK(glossary_lookup("no such term") == 0);
}

#ifdef KC_SELFTEST
/* Run every lesson headless: any crash, fault or hang fails the build. */
static void test_lessons_run(void) {
    CHECK(learn_lesson_count() >= 10);
    learn_run_all_auto();
}
#endif

/* ---- runner ----------------------------------------------------------- */

struct test_case {
    const char *name;
    void (*run)(void);
};

static const struct test_case tests[] = {
    { "lib/string",           test_string },
    { "lib/kprintf",          test_kprintf },
    { "pmm: reservations",    test_pmm_reservations },
    { "pmm: single frames",   test_pmm_single_frames },
    { "pmm: contiguous",      test_pmm_contiguous },
    { "heap: basic",          test_heap_basic },
    { "heap: calloc",         test_heap_calloc },
    { "heap: fragmentation",  test_heap_fragmentation },
    { "heap: exhaustion",     test_heap_exhaustion },
    { "keyboard ring",        test_keyboard_ring },
    { "rtc",                  test_rtc },
    { "glossary",             test_glossary },
#ifdef KC_SELFTEST
    { "lessons run headless", test_lessons_run },
#endif
};

int selftest_run(void) {
    int total = (int)(sizeof(tests) / sizeof(tests[0]));
    int failed = 0;

    kprintf("selftest: running %d tests\n", total);

    for (int i = 0; i < total; i++) {
        int before = check_failures;

        tests[i].run();

        if (check_failures != before) {
            failed++;
            kprintf("[FAIL] %s\n", tests[i].name);
        } else {
            kprintf("[ ok ] %s\n", tests[i].name);
        }
    }

    if (failed == 0) {
        kprintf("selftest: PASS (%d/%d)\n", total, total);
    } else {
        kprintf("selftest: FAIL (%d of %d tests failed)\n", failed, total);
    }

    return failed;
}
