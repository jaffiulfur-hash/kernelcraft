#include "learn/learn.h"
#include "arch/cpu.h"
#include "arch/interrupts.h"
#include "layout.h"
#include "lib/kprintf.h"

/* One 16-byte gate of the 64-bit IDT (same layout as idt.c). */
struct idt_gate {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
} __attribute__((packed));

void lesson_idt(void) {
    learn_page("Exceptions and the IDT");
    learn_heading("How the CPU finds our code");
    learn_para("When something happens - a divide by zero, a key press, an "
               "int3 instruction - the CPU looks up a vector number (0 to 255) "
               "in the Interrupt Descriptor Table. Each 16-byte gate holds the "
               "handler's address, the code segment to run it in, and its "
               "type and privilege. The CPU then pushes SS, RSP, RFLAGS, CS "
               "and RIP (plus an error code for some exceptions), loads CS:RIP "
               "from the gate and runs the handler. An interrupt gate also "
               "clears IF so the handler is not interrupted in turn. The "
               "iretq instruction pops it all back.");
    learn_para("Vectors 0-31 are reserved for CPU exceptions. 32-47 are the "
               "hardware IRQs after we remap the PIC. The rest are free; 0x80 "
               "is the traditional system call vector.");
    learn_para("Our stubs in isr.asm and irq.asm make every vector look the "
               "same: push a dummy error code if the CPU did not, push the "
               "vector number, save all registers, and call C: "
               "interrupt_dispatch(frame). The frame struct in "
               "include/arch/isr.h mirrors that stack layout field by field.");
    LEARN_NEXT();

    struct dtr idtr;

    sidt(&idtr);

    learn_page("Exceptions and the IDT");
    learn_live("the IDT the CPU is using right now (sidt)");
    kprintf("  IDTR base = 0x%016lx   limit = %u (%u gates)\n", idtr.base,
            idtr.limit, (idtr.limit + 1u) / 16u);
    kprintf("  handlers live in .text: 0x%lx - 0x%lx\n\n", (uint64_t)(uintptr_t)__text_start,
            (uint64_t)(uintptr_t)__text_end);
    kprintf("  vec name                     handler            sel  type DPL IST\n");

    static const uint8_t vectors[] = { 0, 3, 8, 13, 14, 32, 33, 128, 200 };
    const struct idt_gate *idt = (const struct idt_gate *)(uintptr_t)idtr.base;

    for (int i = 0; i < LEARN_COUNT(vectors); i++) {
        uint8_t v = vectors[i];
        const struct idt_gate *g = &idt[v];

        if (!(g->attr & 0x80)) {
            kprintf("  %3u %-24s (gate not present)\n", v, interrupt_vector_name(v));
            continue;
        }

        uint64_t handler = (uint64_t)g->offset_low | ((uint64_t)g->offset_mid << 16) |
                           ((uint64_t)g->offset_high << 32);

        kprintf("  %3u %-24s 0x%016lx 0x%02x %-4s %d   %d\n", v,
                interrupt_vector_name(v), handler, g->selector,
                (g->attr & 0xF) == 0xE ? "intr" : "trap", (g->attr >> 5) & 3, g->ist);
    }

    kprintf("\n");
    learn_para("attr 0x8E = present, ring 0, 64-bit interrupt gate. sel 0x08 is "
               "the kernel code segment from the GDT lesson - the two tables "
               "are linked by that number. IST 0 means 'stay on the current "
               "stack'; a real kernel gives double faults their own IST stack.");
    LEARN_NEXT();

    learn_page("Exceptions and the IDT");
    learn_heading("Faults, traps and aborts");
    kprintf("  vec  name                class  error code  pushed RIP points at\n");
    kprintf("   0   Divide Error        fault  no          the failing divide\n");
    kprintf("   3   Breakpoint (int3)   trap   no          the NEXT instruction\n");
    kprintf("   6   Invalid Opcode      fault  no          the bad instruction\n");
    kprintf("   8   Double Fault        abort  yes (zero)  (unrecoverable)\n");
    kprintf("  13   General Protection  fault  yes         the offending instruction\n");
    kprintf("  14   Page Fault          fault  yes + CR2   the failing access\n\n");
    learn_para("A fault can be fixed and the instruction retried - a page fault "
               "handler loads the missing page and returns, and the program "
               "never knows. A trap reports something that already happened "
               "and resumes after it, which is why 'int3' returns. An abort "
               "cannot be resumed at all. If the CPU cannot even deliver a "
               "double fault it resets the machine: a triple fault.");
    learn_try("int3  (trap: resumes)    div0  (fault: we halt)    pf  (page fault)");
    learn_try("then 'regs' to inspect the saved frame, or 'learn irq' for counters.");
}
