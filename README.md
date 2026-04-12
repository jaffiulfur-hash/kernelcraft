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
- later: paging, heap, tasks, VFS, syscalls

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
- keyboard input
- kernel shell
- debug trap support
- Multiboot2 memory map parsing
- physical memory manager groundwork

## Project Structure

```text
kernelcraft/
├── Makefile
├── linker.ld
├── README.md
├── LICENSE
├── .gitignore
├── iso/
├── include/
│   ├── arch/
│   ├── boot/
│   └── mm/
└── src/
    ├── arch/
    ├── boot/
    └── mm/
