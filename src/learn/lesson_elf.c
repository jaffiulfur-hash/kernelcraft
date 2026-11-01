#include "learn/learn.h"
#include "lib/kprintf.h"
#include "lib/string.h"
#include "mm/aspace.h"
#include "mm/paging.h"
#include "proc/elf.h"
#include "proc/process.h"

void lesson_elf(void) {
    const struct user_program *hello = process_find_program("hello");

    learn_page("Executables: the ELF format");
    learn_heading("A program is a file that says where its bytes go");
    learn_para("The kernel cannot run a program until it knows which bytes are "
               "code, which are read-only data, which are variables, and "
               "where in memory each belongs. An executable file carries that "
               "map. On Linux, and here, the format is ELF.");
    learn_para("An ELF file starts with a header (magic 0x7F 'E' 'L' 'F', 32 or "
               "64 bit, CPU type, the ENTRY address) and a table of program "
               "headers. Each PT_LOAD entry says: take filesz bytes from "
               "this offset in the file, put them at this virtual address, "
               "and make memsz bytes available (the extra is zeroed: that is "
               ".bss), with these permissions. Loading is just that loop.");
    learn_para("user/user.ld places code, read-only data and data in "
               "separate page-aligned segments, so each page can get exactly "
               "its own permissions. The Makefile compiles user/prog_*.c, "
               "links each into an ELF, and tools/mkblob.sh embeds them in "
               "the kernel image - there is no filesystem yet.");
    learn_try("write user/prog_mine.c with int main(void), run make, then 'run mine'");
    LEARN_NEXT();

    learn_page("Executables: the ELF format");
    learn_live("parsing the embedded user/prog_hello.c executable");
    if (!hello || elf_check(hello->start, (size_t)(hello->end - hello->start))) {
        kprintf("  program not available\n");
        return;
    }

    const struct elf64_ehdr *h = (const struct elf64_ehdr *)hello->start;

    kprintf("  first bytes:");
    for (int i = 0; i < 16; i++) {
        kprintf(" %02x", hello->start[i]);
    }
    kprintf("\n  = 7f 'E' 'L' 'F', class %d (64-bit), data %d (little-endian)\n", h->ident[4],
            h->ident[5]);
    kprintf("  type %u (executable)  machine 0x%x (x86_64)  entry 0x%lx\n", h->type, h->machine,
            h->entry);
    kprintf("  %u program headers at file offset %lu, file size %lu bytes\n\n", h->phnum,
            h->phoff, (uint64_t)(hello->end - hello->start));
    kprintf("  type  offset   vaddr        filesz  memsz   flags\n");
    for (int i = 0; i < h->phnum; i++) {
        const struct elf64_phdr *ph =
            (const struct elf64_phdr *)(hello->start + h->phoff + (uint64_t)i * h->phentsize);

        kprintf("  %-5s 0x%05lx  0x%08lx  %6lu  %6lu  %c%c%c%s\n",
                ph->type == ELF_PT_LOAD ? "LOAD" : "other", ph->offset, ph->vaddr, ph->filesz,
                ph->memsz, (ph->flags & ELF_PF_R) ? 'r' : '-', (ph->flags & ELF_PF_W) ? 'w' : '-',
                (ph->flags & ELF_PF_X) ? 'x' : '-', ph->memsz == 0 ? "  (empty: no data)" : "");
    }
    kprintf("\n");
    learn_para("Segment 1 is the code at the entry point, r-x. Segment 2 is "
               "read-only data (string literals), r--. Segment 3 would hold "
               "variables, rw-; hello has none, so it is empty and the loader "
               "skips it. Compare 'isolate', which has a real data segment.");
    LEARN_NEXT();

    learn_page("Executables: the ELF format");
    learn_live("the loader mapping it, and refusing bad files");
    struct aspace *as = aspace_create();
    uint64_t entry = 0;
    const char *err = as ? elf_load(as, hello->start, (size_t)(hello->end - hello->start), &entry)
                         : "out of memory";

    if (err) {
        kprintf("  load failed: %s\n", err);
    } else {
        kprintf("  loaded; entry = 0x%lx. Pages now mapped (read from the page table):\n", entry);
        for (int i = 0; i < as->nframes; i++) {
            struct paging_walk w;
            char flags[40];

            for (uint64_t va = USER_BASE; va < USER_LIMIT; va += 0x1000) {
                paging_walk_cr3(as->cr3, va, &w);
                if (w.mapped && w.phys == as->frames[i]) {
                    paging_format_flags(w.entry[3], flags, sizeof(flags));
                    kprintf("    0x%lx -> 0x%lx  %s\n", va, w.phys, flags);
                    break;
                }
            }
        }
    }
    aspace_destroy(as);

    kprintf("\n  hostile files:\n");
    uint8_t buf[64 + 56];

    memcpy(buf, hello->start, sizeof(buf));
    buf[1] = 'X';
    kprintf("    corrupted magic        -> %s\n", elf_check(buf, sizeof(buf)));

    memcpy(buf, hello->start, sizeof(buf));
    ((struct elf64_ehdr *)buf)->machine = 0x28;
    kprintf("    ARM binary             -> %s\n", elf_check(buf, sizeof(buf)));

    memcpy(buf, hello->start, sizeof(buf));
    struct elf64_ehdr *eh = (struct elf64_ehdr *)buf;
    struct elf64_phdr *ph = (struct elf64_phdr *)(buf + eh->phoff);

    eh->phnum = 1;
    ph->offset = 0;
    ph->filesz = 0;
    ph->memsz = 4096;
    ph->vaddr = 0x100000;
    as = aspace_create();
    kprintf("    segment at the kernel  -> %s\n",
            as ? elf_load(as, buf, sizeof(buf), &entry) : "out of memory");
    aspace_destroy(as);
    kprintf("\n");
    learn_para("An ELF file is input from outside the trust boundary, like a "
               "pointer from user mode: every offset and size is range-checked "
               "before it is used. Skipping those checks is how 'open a "
               "crafted file, get kernel code execution' bugs happen.");
}
