#include "proc/elf.h"
#include "lib/string.h"

const char *elf_check(const uint8_t *img, size_t size) {
    const struct elf64_ehdr *h = (const struct elf64_ehdr *)img;

    if (size < sizeof(*h)) return "file too small";
    if (img[0] != 0x7F || img[1] != 'E' || img[2] != 'L' || img[3] != 'F') return "bad magic";
    if (h->ident[4] != 2) return "not a 64-bit ELF";
    if (h->ident[5] != 1) return "not little-endian";
    if (h->type != 2) return "not an executable";
    if (h->machine != 0x3E) return "not x86_64";
    if (h->phentsize != sizeof(struct elf64_phdr)) return "unexpected program header size";
    if (h->phoff > size || (uint64_t)h->phnum * h->phentsize > size - h->phoff) {
        return "program headers lie outside the file";
    }

    return 0;
}

const char *elf_load(struct aspace *as, const uint8_t *img, size_t size, uint64_t *entry) {
    const char *why = elf_check(img, size);

    if (why) {
        return why;
    }

    const struct elf64_ehdr *h = (const struct elf64_ehdr *)img;
    const uint64_t stack_bottom = USER_STACK_TOP - (uint64_t)USER_STACK_PAGES * 4096;

    for (int i = 0; i < h->phnum; i++) {
        const struct elf64_phdr *ph =
            (const struct elf64_phdr *)(img + h->phoff + (uint64_t)i * h->phentsize);

        if (ph->type != ELF_PT_LOAD || ph->memsz == 0) {
            continue;
        }

        /* Never trust the file: every field is range-checked before use. */
        if (ph->filesz > ph->memsz) return "segment file size exceeds memory size";
        if (ph->offset > size || ph->filesz > size - ph->offset) return "segment data lies outside the file";
        if (ph->vaddr < USER_BASE || ph->vaddr >= stack_bottom ||
            ph->memsz > stack_bottom - ph->vaddr) {
            return "segment lies outside the user window";
        }

        int writable = (ph->flags & ELF_PF_W) != 0;
        int executable = (ph->flags & ELF_PF_X) != 0;
        uint64_t first = ph->vaddr & ~0xFFFull;
        uint64_t end = (ph->vaddr + ph->memsz + 0xFFF) & ~0xFFFull;

        for (uint64_t va = first; va < end; va += 4096) {
            uint64_t frame = aspace_map_new_page(as, va, writable, executable);

            if (!frame) {
                return "out of memory, or two segments share a page";
            }

            /* Copy the part of the file that lands in this page (frame is zeroed,
             * so the .bss tail and padding are already zero). */
            uint64_t lo = va > ph->vaddr ? va : ph->vaddr;
            uint64_t file_end = ph->vaddr + ph->filesz;
            uint64_t hi = va + 4096 < file_end ? va + 4096 : file_end;

            if (hi > lo) {
                memcpy((void *)(uintptr_t)(frame + (lo - va)),
                       img + ph->offset + (lo - ph->vaddr), hi - lo);
            }
        }
    }

    if (h->entry < USER_BASE || h->entry >= stack_bottom) {
        return "entry point outside the user window";
    }

    *entry = h->entry;
    return 0;
}
