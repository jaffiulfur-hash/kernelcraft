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
- virtual memory (page-table walking), threads and a scheduler
- system calls, PCI discovery
- user mode: ring 3, the TSS, per-process address spaces, an ELF loader
- later: a filesystem, disk drivers, fork

## Learn by running it

KernelCraft is a kernel that teaches itself. Boot it and type:

| Command         | What you get                                                        |
| --------------- | ------------------------------------------------------------------- |
| `learn`         | the list of 17 lessons, from boot to user-mode processes            |
| `learn heap`    | one lesson: explanation plus **live** data from the running machine |
| `tour`          | every lesson in order                                               |
| `quiz`          | 10 random questions with explanations                               |
| `glossary page fault` | look up an OS term                                            |

Lessons do not show diagrams of a GDT, a page table or a heap - they decode
the real ones. The scheduler lesson creates real threads and prints their
state; the paging lesson walks the actual page tables for any address
(`vtop 0xB8000`). See [docs/curriculum.md](docs/curriculum.md) for the full
course map, the source files to read alongside each lesson, and an exercise
for each.

## Current Progress

Implemented stages so far:

- bootable kernel with GRUB + Multiboot2
- 32-bit to 64-bit long mode transition
- VGA text output and serial logging
- GDT, IDT, exceptions and IRQ handling, PIC remap
- PIT timer, RTC clock, PS/2 keyboard (Shift, Caps Lock, ring buffer)
- kernel shell (table-driven, with arguments) running outside interrupt context
- debug traps and decoded page-fault reports
- Multiboot2 parsing; physical memory manager (bitmap, contiguous allocation,
  reserved-frame protection)
- read-only page-table walker
- kernel heap (`kmalloc` / `kfree`, first fit with split and coalesce)
- kernel threads and a preemptive round-robin scheduler, sleep/join, mutexes
- `int 0x80` system calls through a DPL 3 gate
- **user mode**: real ring 3 processes, each with its own address space, loaded
  from ELF executables; faults kill the process, not the kernel
- PCI enumeration
- freestanding library: `string.h` helpers and `kprintf`
- 17 interactive lessons, a glossary and a quiz
- self-tests (including every lesson, run headless) for `make test`

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
| `make test`  | boot a self-test build headless; runs every test and lesson           |
| `make clean` | remove `build/`                                                       |

Every `.c` and `.asm` file under `src/` is picked up automatically, and
editing a header rebuilds whatever includes it. Adding a source file needs no
Makefile change.

## The shell

After boot you get a `craft>` prompt. Numbers can be decimal or `0x` hex.
Type `help` for the full list (generated from the command table in
`src/shell.c`).

| Command                | Description                                        |
| ---------------------- | -------------------------------------------------- |
| `learn`, `tour`, `quiz`, `glossary` | the curriculum (see above)            |
| `mem`, `frames`, `mmap`| physical memory summary, free/used, firmware map   |
| `alloc [n]`, `free <addr> [n]` | physical frames (refuses reserved ones)    |
| `kmalloc <bytes>`, `kfree <addr>`, `heap [dump]` | the kernel heap          |
| `vtop <addr>`          | walk the page tables for an address                |
| `progs`, `run <prog>...`, `spawn <prog>`, `kill <pid>` | user-mode programs   |
| `ps`                   | threads and scheduler statistics                   |
| `pci`                  | list PCI devices                                   |
| `hexdump <addr> [len]` | dump memory (first 1 GiB only)                     |
| `date`, `uptime`, `ticks`, `sleep <ms>` | clocks and waiting                |
| `int3`, `div0`, `pf`, `panic`, `regs` | traps, faults and the saved frame   |
| `selftest`             | run the kernel self-tests                          |
| `echo`, `clear`, `about`, `reboot`, `halt` | the basics                     |

## User programs

`user/prog_<name>.c` files are compiled into real ELF executables, linked at
`0x40000000`, embedded in the kernel image, and run in ring 3:

```text
craft> run hello                  a program in ring 3, via int 0x80
craft> run isolate isolate        same virtual address, different memory
craft> run crash_kernel           reads kernel memory: killed by a page fault
craft> run privileged             executes `cli`: killed by a protection fault
craft> spawn spin  /  ps  /  kill 8     a runaway loop is preempted, then ended
```

To add one, create `user/prog_mine.c` with `int main(void)` (the runtime in
`user/ulib.h` has `uputs`, `ugetpid`, `usleep_ms`, ...), run `make`, then
`run mine`. No Makefile edit is needed.

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
│   ├── curriculum.md          the lessons, source files to read, exercises
│   └── memory.md              how the PMM and heap work
├── user/                      user-mode programs, runtime and linker script
├── tools/                     mkblob.sh: embeds the ELFs in the kernel
├── iso/
├── include/
│   ├── arch/                  x86_64 hardware: GDT, IDT, PIC, PIT, RTC, PCI, ...
│   ├── boot/                  Multiboot2 structures and boot info
│   ├── learn/                 the lesson framework
│   ├── lib/                   string helpers and kprintf
│   ├── mm/                    pmm.h, heap.h, paging.h, aspace.h
│   ├── proc/                  ELF loader and user processes
│   ├── sched/                 threads, scheduler, mutex
│   └── syscall/               system call numbers and ABI
└── src/
    ├── arch/x86_64/           boot.asm, interrupts, drivers
    ├── boot/
    ├── learn/                 one file per lesson, plus glossary and quiz
    ├── lib/
    ├── mm/                    pmm, heap, paging walker, per-process address spaces
    ├── proc/                  elf.c, process.c, usermode.asm (iretq into ring 3)
    ├── sched/                 thread.c and switch.asm (the context switch)
    ├── syscall/
    ├── kmain.c                boot sequence and the main loop
    ├── shell.c                command table and input handling
    └── selftest.c             tests for the kernel and the curriculum
```

## Documentation

- [docs/curriculum.md](docs/curriculum.md) - the lessons and how to study them
- [docs/memory.md](docs/memory.md) - physical memory manager and kernel heap
