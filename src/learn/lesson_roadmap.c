#include "learn/learn.h"
#include "lib/kprintf.h"

void lesson_roadmap(void) {
    learn_page("How an operating system is built");
    learn_heading("What an operating system actually does");
    learn_para("An operating system is the one program that is allowed to touch "
               "the hardware directly. Everything else asks it politely. Four "
               "jobs make up almost everything a kernel does:");
    kprintf("  1. Share the CPU      scheduling: many tasks, few processors\n");
    kprintf("  2. Share the memory   allocation and isolation between programs\n");
    kprintf("  3. Hide the hardware  drivers and files instead of port numbers\n");
    kprintf("  4. Offer an interface system calls: the only door into the kernel\n\n");
    learn_para("KernelCraft builds the foundations of jobs 1 to 3 in a few "
               "thousand lines of readable C and assembly. Each lesson explains "
               "one piece, then shows the live state of the machine you are "
               "running on - not a diagram, the real thing.");
    learn_try("learn  (list lessons)    glossary  (look up a term)");
    LEARN_NEXT();

    learn_page("How an operating system is built");
    learn_heading("The order you build things in");
    kprintf(" stage what you build                   files\n");
    kprintf(" ----- -------------------------------- -------------------------\n");
    kprintf(" 1     boot, 64-bit mode, text output   boot.asm linker.ld vga.c\n");
    kprintf(" 2     GDT, IDT, exceptions             gdt.c idt.c isr.asm\n");
    kprintf(" 3     PIC, timer, keyboard, shell      pic.c pit.c keyboard.c\n");
    kprintf(" 4     physical memory manager          mm/pmm.c\n");
    kprintf(" 4.1   kernel heap, kprintf, tests      mm/heap.c lib/ selftest.c\n");
    kprintf(" 5     lessons, page-table walker       learn/ mm/paging.c\n");
    kprintf(" 5.2   threads and a scheduler          sched/\n");
    kprintf(" 5.3   PCI scan, system calls           pci.c syscall/\n");
    kprintf(" next  user mode, filesystem, drivers   (your turn)\n\n");
    learn_para("The order is not arbitrary. You cannot handle a fault until the "
               "IDT exists, cannot allocate until you know what memory you have, "
               "and cannot schedule threads until you can allocate their stacks.");
    LEARN_NEXT();

    learn_page("How an operating system is built");
    learn_heading("Where KernelCraft stops, and how to study it");
    learn_para("A production kernel adds: user mode with a separate address "
               "space per process, demand-paged 4 KiB mappings, a virtual file "
               "system with disk drivers, multiple CPUs and the APIC, ACPI power "
               "management, a network stack, and a security model. Each one "
               "builds on exactly the pieces you can inspect here.");
    learn_para("A good way to use this repository:\n"
               "1. run a lesson and watch the live data\n"
               "2. open the source file it points to\n"
               "3. change one thing (a constant, a message, a new shell command)\n"
               "4. make run and see what happens");
    learn_para("Break things on purpose. A kernel is the best place to learn "
               "that a one-line mistake means a triple fault and a reboot.");
}
