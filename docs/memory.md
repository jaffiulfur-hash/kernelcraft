# Memory management

This page describes how KernelCraft OS manages memory at stage 4.1. All of it
lives in `src/mm/` and `include/mm/`.

## Boot-time memory layout

`boot.asm` switches the CPU to long mode and **identity-maps the first 1 GiB**
using 2 MiB pages, so a physical address and the virtual address used to
reach it are the same number. Everything below treats that as a given, which
is why the heap can hand out raw PMM addresses as pointers. Replacing this
mapping with real paging is a later stage.

```text
0x00000000 +-----------------------------+
           | legacy low memory (1 MiB)   |  BIOS data, VGA text at 0xB8000, ROMs
0x00100000 +-----------------------------+  __kernel_start  (linker.ld)
           | kernel image                |  .multiboot .text .rodata .data .bss
           |  (stack, page tables,       |  (the PMM bitmaps live in .bss too)
           |   PMM bitmaps)              |
           +-----------------------------+  __kernel_end
           | Multiboot2 info block       |  placed by GRUB; reserved by kmain
           +-----------------------------+
           | kernel heap (1 MiB)         |  allocated from the PMM at boot
           | free frames ...             |
0x08000000 +-----------------------------+  PMM_MAX_MEMORY_BYTES (128 MiB)
```

The diagram is schematic: GRUB decides where the Multiboot2 info block goes,
and the heap lands wherever the first run of 256 free frames starts (use `mmap`,
`mem` and `heap dump` in the shell to see the real numbers).

`__kernel_start` and `__kernel_end` are symbols defined in `linker.ld` and
read through `include/layout.h`. The exact positions move whenever code is
added, which is why nothing hard-codes them.

## Physical memory manager (`pmm.c`)

The PMM tracks the first 128 MiB of RAM, one bit per 4 KiB frame. Any RAM
above that is simply left alone.

Each frame is in one of three states, kept in two bitmaps:

| State     | `used_map` | `reserved_map` | Meaning                                     |
| --------- | :--------: | :------------: | ------------------------------------------- |
| free      |     0      |       0        | can be returned by `pmm_alloc_*`            |
| reserved  |     1      |       1        | permanent: never handed out, never freeable |
| allocated |     1      |       0        | handed out; can be given back               |

`pmm_init()` follows these steps:

1. mark **every** frame reserved (so unknown memory is never used by accident);
2. walk the Multiboot2 memory map and free frames that lie completely inside
   an `available` region;
3. reserve the legacy low 1 MiB and the kernel image.

`kmain` then calls `pmm_reserve_range()` for the Multiboot2 info block, which
GRUB places in ordinary RAM and which also holds the memory map the PMM
keeps a pointer to.

Why a separate reserved bitmap? So that `pmm_free_frame()` can refuse to
free the kernel image or BIOS memory - a typo in the shell's `free` command
must not be able to hand the kernel's own code to the allocator.

`pmm_alloc_contiguous(n)` is first fit: scan for `n` consecutive free frames,
skipping fully used bytes (8 frames) at a time. Failure returns `0`, which
cannot be a valid result because frame 0 is always reserved.

## Kernel heap (`heap.c`)

The heap takes one contiguous region of `HEAP_FRAMES` (256, so 1 MiB) frames
from the PMM at boot and carves it up with a first-fit allocator.

```text
region start
+--------+----------------+--------+------------+--------+-----------------+
| header | payload (used) | header | payload    | header | payload (free)  |
+--------+----------------+--------+------------+--------+-----------------+
```

- A 32-byte header precedes every block: a magic number, the payload size,
  and `prev`/`next` links. Blocks form one address-ordered list that tiles
  the region exactly.
- Payload sizes are rounded up to 16 bytes, so every pointer is 16-byte aligned.
- `kmalloc` takes the first free block that is large enough, **splitting** it
  if the remainder can hold another block.
- `kfree` marks the block free and **coalesces** it with free neighbours
  (next, then previous), so freeing everything restores one big block.
- Magic numbers catch corruption. `kfree` panics on a pointer it never
  returned and on a double free; `heap_check()` validates every header and
  every link and is used by the self-tests and the `heap` shell command.

Known limits, all deliberate for now:

- **Fixed size.** Growing would need new contiguous frames adjacent to the
  old region; that waits for paging.
- **`kfree` is O(n)**: it walks the list to confirm the pointer is genuine
  instead of trusting the header in front of it.
- **Locking is "interrupts off".** `kmalloc` and `kfree` disable interrupts
  while they run, which is a complete lock on one CPU (nothing can preempt
  them). It would not be enough with several CPUs.

## Self-tests

`src/selftest.c` checks the string helpers, `kprintf`, the PMM (including bad
frees and contiguous runs), the heap (alignment, overlap, reuse, coalescing,
exhaustion), the scheduler (round-robin order, sleep, timer preemption, race
conditions and their fixes), PCI, syscalls, and - in the `make test` build -
runs every lesson headless. Run it with the shell's `selftest` command, or with
`make test`, which boots a `-DKC_SELFTEST` build headless and reads the result
from QEMU's exit status via the `isa-debug-exit` device.
