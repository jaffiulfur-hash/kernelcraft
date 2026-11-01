# The KernelCraft curriculum

KernelCraft teaches operating-system development with the kernel itself. Boot
it (`make run`) and type `learn`. Each lesson alternates **explanation** with
**live sections** that read the real state of the machine you are running on:
the GDT the CPU is using, the page tables it is walking, the heap as it
splits and merges, the threads being scheduled.

## How to study

1. Run a lesson (`learn <name>`; Enter for the next page, `q` to quit).
2. Open the source files listed for it and find the code that produced the
   live data.
3. Do the exercise. Break something on purpose and predict what happens.
4. Look up unfamiliar words with `glossary <term>`, and test yourself with
   `quiz`. `tour` runs every lesson in order.

`make test` boots the kernel headless and runs every lesson plus the
self-tests, so the curriculum cannot silently rot: if a lesson crashes, the
build fails.

## The lessons

| # | Lesson | You will learn | Read | Exercise |
|---|--------|----------------|------|----------|
| 1 | `roadmap` | what an OS does; the order to build one | README, `kmain.c` | Add a shell command that prints your name. |
| 2 | `boot` | firmware, GRUB, Multiboot2, real mode to long mode | `boot.asm`, `linker.ld`, `boot/multiboot2.c` | Swap `heap_init()` and `pmm_init()` in `kmain.c`; explain the result. |
| 3 | `cpu` | rings, control registers, CPUID, RFLAGS | `arch/cpu.h` | Add a `cpuid` shell command. |
| 4 | `gdt` | segmentation, selectors, descriptor bit layout | `arch/x86_64/gdt.c`, `gdt_flush.asm` | Make the data segment read-only and see what faults first. |
| 5 | `idt` | gates, exceptions vs traps vs aborts, the ISR stubs | `idt.c`, `isr.asm`, `interrupts.c` | Add a handler for vector 6 (invalid opcode) that prints the bad `rip`. |
| 6 | `irq` | PIC, EOI, masking, the PIT, calibrating with the RTC | `pic.c`, `pit.c`, `rtc.c` | Reprogram the PIT to 1000 Hz and check the measurement. |
| 7 | `keyboard` | scancodes, the IRQ top half and the ring buffer | `keyboard.c` | Add Ctrl-L to clear the screen. |
| 8 | `pmm` | frame bitmap, reservations, first fit, fragmentation | `mm/pmm.c` | Make allocation search downward from the top of RAM. |
| 9 | `paging` | four-level page tables, huge pages, faults | `mm/paging.c`, `boot.asm` | Write `paging_map()` for 4 KiB pages and map VGA elsewhere. |
| 10 | `heap` | split, coalesce, fragmentation | `mm/heap.c` | Turn first fit into best fit and compare the lesson's drawing. |
| 11 | `sched` | threads, context switch, preemption, round robin | `sched/thread.c`, `sched/switch.asm` | Add thread priorities. |
| 12 | `sync` | race conditions, mutexes, atomics, deadlock | `sched/sync.h` | Construct a deadlock with two mutexes, then fix it. |
| 13 | `syscall` | the kernel entry point, ABI, distrusting arguments | `syscall/syscall.c` | Add a `SYS_GETTICKS` call. |
| 14 | `pci` | hardware discovery, config space, BARs | `arch/x86_64/pci.c` | Read a register from the network card's BAR. |

A good order for a first pass is simply 1 to 14: each lesson only relies on
the ones before it.

## What is real, and what is not

Everything the live sections print is read from hardware or kernel state at
that moment - nothing is canned. Two honest limits:

- **No user mode yet.** `int 0x80` works, but the caller is kernel code in
  ring 0, so the CPU does not change privilege. The `syscall` lesson says so
  and explains the missing pieces (GDT user segments, TSS, US page bits, a
  DPL 3 gate).
- **One CPU, no filesystem or disk driver.** Locking is "disable interrupts",
  and there is nothing persistent. These are natural next stages.

## Adding a lesson

1. Create `src/learn/lesson_<name>.c` with `void lesson_<name>(void)`.
2. Use `learn_page()`, `learn_para()`, `learn_live()` and `LEARN_NEXT()` from
   `include/learn/learn.h`. Keep a page under about 22 lines; the layout check
   used during development measured every page against the 25-row screen.
3. Declare the function in `learn.h` and add one line to `lessons[]` in
   `src/learn/learn.c`.
4. `make test`: it runs your lesson headless, so any crash shows up at once.

If your lesson allocates frames, heap blocks or threads, release them on every
exit path, including the learner pressing `q` (see `LEARN_NEXT_GOTO` in
`lesson_pmm.c`).
