#ifndef LEARN_LEARN_H
#define LEARN_LEARN_H

#include <stdint.h>

/*
 * The built-in curriculum. A lesson is a function that shows a few pages of
 * explanation mixed with LIVE sections that read the real state of the
 * running machine (the actual GDT, page tables, heap, threads, ...).
 *
 * Writing a lesson:
 *   1. create src/learn/lesson_<name>.c with `void lesson_<name>(void)`
 *   2. declare it below and add one line to lessons[] in learn.c
 * That is all; the Makefile finds new files by itself.
 */

struct lesson {
    const char *name;
    const char *title;
    const char *summary;
    void (*run)(void);
};

/* ---- the `learn` shell command and friends ---------------------------- */
void learn_command(int argc, char **argv);
void learn_run_all_auto(void);             /* non-interactive, for make test */
int learn_lesson_count(void);

/* ---- helpers for writing lessons -------------------------------------- */

/* Start a new page: clears the screen and prints the header bar. */
void learn_page(const char *title);

/* Word-wrapped paragraph followed by a blank line. '\n' forces a line break. */
void learn_para(const char *text);
void learn_heading(const char *text);
void learn_live(const char *what);         /* marks a section as live data */
void learn_try(const char *text);          /* a suggestion to try at the shell */

/* Footer + wait. Returns 1 to continue, 0 if the learner pressed q. */
int learn_next(void);
int learn_is_auto(void);

/* Decode a register into the names of its set bits. */
struct learn_flag {
    uint8_t bit;
    const char *name;
};
void learn_print_flags(uint64_t value, const struct learn_flag *flags, int count);

#define LEARN_COUNT(array) ((int)(sizeof(array) / sizeof((array)[0])))

#define LEARN_NEXT() \
    do { if (!learn_next()) return; } while (0)
#define LEARN_NEXT_GOTO(label) \
    do { if (!learn_next()) goto label; } while (0)

/* ---- the lessons ------------------------------------------------------ */
void lesson_roadmap(void);
void lesson_boot(void);
void lesson_cpu(void);
void lesson_gdt(void);
void lesson_idt(void);
void lesson_irq(void);
void lesson_keyboard(void);
void lesson_pmm(void);
void lesson_paging(void);
void lesson_heap(void);

/* ---- glossary --------------------------------------------------------- */
void glossary_command(int argc, char **argv);
int glossary_term_count(void);
const char *glossary_lookup(const char *term);     /* NULL if unknown */

#endif
