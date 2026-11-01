#ifndef PROC_ELF_H
#define PROC_ELF_H

#include <stddef.h>
#include <stdint.h>
#include "mm/aspace.h"

/* The parts of the ELF64 format a loader needs. */
struct elf64_ehdr {
    uint8_t  ident[16];      /* 0x7F 'E' 'L' 'F', class, endianness, ... */
    uint16_t type;           /* 2 = executable */
    uint16_t machine;        /* 0x3E = x86_64 */
    uint32_t version;
    uint64_t entry;          /* virtual address of the first instruction */
    uint64_t phoff;          /* where the program headers are in the file */
    uint64_t shoff;
    uint32_t flags;
    uint16_t ehsize, phentsize, phnum, shentsize, shnum, shstrndx;
} __attribute__((packed));

struct elf64_phdr {
    uint32_t type;           /* 1 = PT_LOAD: map this part of the file */
    uint32_t flags;          /* 1 = execute, 2 = write, 4 = read */
    uint64_t offset;         /* where its bytes start in the file */
    uint64_t vaddr;          /* where they go in memory */
    uint64_t paddr;
    uint64_t filesz;         /* bytes to copy from the file */
    uint64_t memsz;          /* bytes of memory (the rest is zeroed: .bss) */
    uint64_t align;
} __attribute__((packed));

#define ELF_PT_LOAD 1
#define ELF_PF_X 1
#define ELF_PF_W 2
#define ELF_PF_R 4

/* NULL if `image` looks like a loadable x86_64 ELF executable, else why not. */
const char *elf_check(const uint8_t *image, size_t size);

/*
 * Map every PT_LOAD segment into `as` with permissions taken from the segment
 * flags. Returns NULL and sets *entry on success, otherwise an error string.
 */
const char *elf_load(struct aspace *as, const uint8_t *image, size_t size, uint64_t *entry);

#endif
