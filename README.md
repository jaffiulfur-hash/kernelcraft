# KernelCraft OS

KernelCraft OS is a small educational operating system written in **C** and **x86_64 assembly**.

Its goal is to help people learn how operating systems and kernels work by building one step by step.

## Goals

KernelCraft OS is designed to be:

- small
- readable
- hackable
- educational
- easy to run in QEMU
- organized in learning stages

Instead of hiding low-level details, KernelCraft OS exposes them clearly:

- boot
- long mode entry
- GDT and IDT
- exceptions and IRQs
- timer and keyboard input
- shell and debug traps
- physical memory management
- kernel heap
- later: paging, tasks, VFS, syscalls

## Current Progress

Implemented stages so far:

- bootable kernel with GRUB + Multiboot2
- 32-bit to 64-bit long mode transition
- VGA text output
- serial logging
- GDT setup
- IDT setup
- exceptions and IRQ handling
- PIT timer
- keyboard input (Shift and Caps Lock)
- kernel shell (table-driven, with arguments)
- debug trap support
- Multiboot2 memory map parsing
- physical memory manager (bitmap, contiguous allocation, reserved-frame protection)
- kernel heap (`kmalloc` / `kfree`, first fit with split and coalesce)
- freestanding library: `string.h` helpers and `kprintf`
- built-in self-tests, runnable from the shell or headless in QEMU

## Building and running

You need `gcc`, `ld`, `nasm`, `grub-mkrescue` (with `xorriso` and `mtools`) and
`qemu-system-x86_64`. On Debian/Ubuntu:

```sh
sudo apt install build-essential nasm qemu-system-x86 grub-pc-bin grub-common xorriso mtools
```

| Command      | What it does                                                          |
| ------------ | --------------------------------------------------------------------- |
| `make`       | build `build/kernel.elf` and the bootable `build/kernelcraft.iso`     |
| `make run`   | boot the ISO in QEMU (serial log also goes to your terminal)          |
| `make debug` | boot paused with a GDB stub on `:1234`                                |
| `make test`  | boot a self-test build headless; passes if every kernel test passes   |
| `make clean` | remove `build/`                                                       |

Every `.c` and `.asm` file under `src/` is picked up automatically, and
editing a header rebuilds whatever includes it. Adding a source file needs no
Makefile change.

## The shell

After boot you get a `craft>` prompt. Numbers can be decimal or `0x` hex.
Type `help` for the full list, generated from the command table in
`src/shell.c`.

| Command                | Description                                        |
| ---------------------- | -------------------------------------------------- |
| `mem`, `frames`        | physical memory summary / free and used frames     |
| `mmap`                 | firmware memory map and the kernel image range     |
| `alloc [n]`            | allocate `n` contiguous physical frames            |
| `free <addr> [n]`      | free frames (refuses reserved or unallocated ones) |
| `kmalloc <bytes>`      | allocate from the kernel heap                      |
| `kfree <addr>`         | free a heap allocation                             |
| `heap [dump]`          | heap statistics, and every block with `dump`       |
| `hexdump <addr> [len]` | dump memory (first 1 GiB only)                     |
| `selftest`             | run the kernel self-tests                          |
| `int3`, `div0`, `panic`| trigger a trap, an exception, a kernel panic       |
| `regs`                 | registers saved by the last trap                   |
| `uptime`, `ticks`      | time since boot                                    |
| `echo`, `clear`, `about`, `reboot`, `halt` | the basics                     |

To add a command, write `static void cmd_foo(int argc, char **argv)` and add
one line to the `commands[]` table.

## Project Structure

```text
kernelcraft/
├── Makefile
├── linker.ld
├── README.md
├── LICENSE
├── .gitignore
├── .github/workflows/ci.yml   build + `make test` on every push
├── docs/
│   └── memory.md              how the PMM and heap work
├── iso/
├── include/
│   ├── arch/                  x86_64 hardware: GDT, IDT, PIC, PIT, serial, ...
│   ├── boot/                  Multiboot2 structures
│   ├── lib/                   string helpers and kprintf
│   └── mm/                    pmm.h, heap.h
└── src/
    ├── arch/x86_64/
    ├── boot/
    ├── lib/
    ├── mm/
    ├── kmain.c                boot sequence
    ├── shell.c                command table and input handling
    └── selftest.c             PMM, heap, string and kprintf tests
```

## Documentation

- [docs/memory.md](docs/memory.md) - physical memory manager and kernel heap
