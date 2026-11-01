#include "learn/learn.h"
#include "arch/cpu.h"
#include "lib/kprintf.h"
#include "lib/string.h"

static const struct learn_flag cr0_flags[] = {
    {0, "PE"}, {1, "MP"}, {2, "EM"}, {3, "TS"}, {4, "ET"}, {5, "NE"},
    {16, "WP"}, {18, "AM"}, {29, "NW"}, {30, "CD"}, {31, "PG"},
};
static const struct learn_flag cr4_flags[] = {
    {0, "VME"}, {3, "DE"}, {4, "PSE"}, {5, "PAE"}, {6, "MCE"}, {7, "PGE"},
    {9, "OSFXSR"}, {10, "OSXMMEXCPT"}, {20, "SMEP"}, {21, "SMAP"},
};
static const struct learn_flag efer_flags[] = {
    {0, "SCE"}, {8, "LME"}, {10, "LMA"}, {11, "NXE"},
};
static const struct learn_flag rflags_flags[] = {
    {0, "CF"}, {2, "PF"}, {4, "AF"}, {6, "ZF"}, {7, "SF"}, {8, "TF"},
    {9, "IF"}, {10, "DF"}, {11, "OF"},
};
static const struct learn_flag feat_edx[] = {
    {0, "FPU"}, {4, "TSC"}, {5, "MSR"}, {6, "PAE"}, {9, "APIC"}, {15, "CMOV"},
    {23, "MMX"}, {25, "SSE"}, {26, "SSE2"},
};
static const struct learn_flag feat_ecx[] = {
    {0, "SSE3"}, {9, "SSSE3"}, {19, "SSE4.1"}, {20, "SSE4.2"}, {28, "AVX"},
    {31, "HYPERVISOR"},
};
static const struct learn_flag feat_ext[] = {
    {11, "SYSCALL"}, {20, "NX"}, {26, "PAGE1GB"}, {29, "LONGMODE"},
};

static void print_reg(const char *name, uint64_t value,
                      const struct learn_flag *flags, int count) {
    kprintf("  %-7s 0x%016lx  ", name, value);
    learn_print_flags(value, flags, count);
    kprintf("\n");
}

void lesson_cpu(void) {
    learn_page("The CPU: modes, registers and CPUID");
    learn_heading("Three modes, four rings");
    learn_para("An x86_64 CPU still boots like an 8086: 16-bit real mode, 1 MiB "
               "of address space, no protection. Protected mode (1985) added "
               "32-bit registers, segments, paging and privilege rings. Long "
               "mode (2003) added 64-bit registers and a flat address space. "
               "Compatibility is why boot.asm has to climb the whole ladder.");
    learn_para("Rings 0 to 3 decide what code may do. Ring 0 can run any "
               "instruction and touch any port or page; ring 3 cannot. Real "
               "systems use only 0 (kernel) and 3 (programs). The ring you are "
               "in is the low two bits of the CS register - the CPL.");
    learn_para("Registers fall into groups: general purpose (rax..r15), the "
               "instruction pointer rip, flags (rflags), segment selectors, "
               "control registers (cr0, cr2, cr3, cr4) that configure the "
               "CPU, and model-specific registers (MSRs) read with rdmsr. "
               "src/arch/cpu.h wraps the privileged ones in one line each.");
    LEARN_NEXT();

    char vendor[13], brand[49];
    uint32_t a, b, c, d;

    cpuid(0, 0, &a, &b, &c, &d);
    memcpy(vendor, &b, 4);
    memcpy(vendor + 4, &d, 4);
    memcpy(vendor + 8, &c, 4);
    vendor[12] = '\0';

    brand[0] = '\0';
    cpuid(0x80000000u, 0, &a, &b, &c, &d);
    if (a >= 0x80000004u) {
        for (uint32_t i = 0; i < 3; i++) {
            uint32_t regs[4];

            cpuid(0x80000002u + i, 0, &regs[0], &regs[1], &regs[2], &regs[3]);
            memcpy(brand + 16 * i, regs, 16);
        }
        brand[48] = '\0';
    }
    const char *brand_text = brand;
    while (*brand_text == ' ') {
        brand_text++;
    }

    learn_page("The CPU: modes, registers and CPUID");
    learn_live("asking the CPU who it is (the cpuid instruction)");
    cpuid(1, 0, &a, &b, &c, &d);
    uint32_t family = (a >> 8) & 0xF, model = (a >> 4) & 0xF;
    if (family == 0xF) {
        family += (a >> 20) & 0xFF;
    }
    if ((((a >> 8) & 0xF) == 6) || (((a >> 8) & 0xF) == 0xF)) {
        model += ((a >> 16) & 0xF) << 4;
    }
    uint32_t feat_c = c, feat_d = d;

    kprintf("  vendor   %s\n", vendor);
    kprintf("  brand    %s\n", brand_text[0] ? brand_text : "(not reported)");
    kprintf("  family %u, model %u, stepping %u\n\n", family, model, a & 0xF);
    kprintf("  features (leaf 1, EDX): ");
    learn_print_flags(feat_d, feat_edx, LEARN_COUNT(feat_edx));
    kprintf("\n  features (leaf 1, ECX): ");
    learn_print_flags(feat_c, feat_ecx, LEARN_COUNT(feat_ecx));
    cpuid(0x80000001u, 0, &a, &b, &c, &d);
    kprintf("\n  features (ext leaf):    ");
    learn_print_flags(d, feat_ext, LEARN_COUNT(feat_ext));
    kprintf("\n\n");
    if (feat_c & (1u << 31)) {
        learn_para("The HYPERVISOR bit is set: this kernel is running inside a "
                   "virtual machine. A hypervisor is itself an operating "
                   "system for operating systems.");
    } else {
        learn_para("No hypervisor bit: this looks like bare metal.");
    }
    LEARN_NEXT();

    learn_page("The CPU: modes, registers and CPUID");
    learn_live("control registers, decoded bit by bit");
    print_reg("CR0", read_cr0(), cr0_flags, LEARN_COUNT(cr0_flags));
    print_reg("CR4", read_cr4(), cr4_flags, LEARN_COUNT(cr4_flags));
    print_reg("EFER", rdmsr(MSR_EFER), efer_flags, LEARN_COUNT(efer_flags));
    print_reg("RFLAGS", read_rflags(), rflags_flags, LEARN_COUNT(rflags_flags));
    kprintf("  CR3     0x%016lx  physical address of the top-level page table\n\n",
            read_cr3());
    kprintf("  CS=0x%02x SS=0x%02x DS=0x%02x   CPL=%d\n\n", read_cs(), read_ss(),
            read_ds(), read_cs() & 3);
    learn_para("PE and PG are protected mode and paging; WP stops the kernel "
               "writing to read-only pages; NXE lets pages be marked "
               "non-executable. IF in RFLAGS is the interrupt flag: cli clears "
               "it, sti sets it - it is what irq_save() manipulates.");
    learn_try("add a 'cpuid' shell command to src/shell.c that prints the vendor.");
}
