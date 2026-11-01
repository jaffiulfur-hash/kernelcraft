#include "learn/learn.h"
#include "arch/interrupts.h"
#include "arch/io.h"
#include "arch/pic.h"
#include "arch/pit.h"
#include "arch/rtc.h"
#include "lib/kprintf.h"

/* Wait (halted, woken by the timer) until the RTC second changes. */
static int wait_for_next_second(void) {
    uint8_t start = rtc_read_seconds();
    uint64_t give_up = pit_get_ticks() + 3 * PIT_HZ;

    while (rtc_read_seconds() == start) {
        if (pit_get_ticks() > give_up) {
            return -1;
        }
        __asm__ volatile ("hlt");
    }

    return 0;
}

void lesson_irq(void) {
    learn_page("Hardware interrupts, the PIC and the timer");
    learn_heading("Devices that tap the CPU on the shoulder");
    learn_para("Polling - asking a device 'anything yet?' in a loop - wastes the "
               "CPU. Instead devices raise an interrupt request (IRQ) line. "
               "The classic PC wires 16 of them to two cascaded 8259 PICs: "
               "the slave's output feeds IRQ 2 of the master. The PIC "
               "decides priority, then signals the CPU, which asks it for a "
               "vector number.");
    learn_para("Two things in pic.c are easy to get wrong. First, the PIC "
               "boots with IRQ 0-7 on vectors 8-15, which collide with CPU "
               "exceptions (8 is the double fault!), so we remap them to "
               "32-47. Second, the PIC will not deliver another interrupt on "
               "a line until the handler sends it an End Of Interrupt (EOI): "
               "forget it and the timer simply stops.");
    learn_para("Each IRQ can be masked in the PIC's interrupt mask register "
               "(IMR). Modern machines use the APIC instead, but still "
               "emulate this PIC, which is why the same code boots in QEMU "
               "and on real hardware.");
    LEARN_NEXT();

    learn_page("Hardware interrupts, the PIC and the timer");
    learn_live("PIC mask registers and interrupt counters");
    uint8_t imr1 = inb(PIC1_DATA), imr2 = inb(PIC2_DATA);

    kprintf("  IMR master = 0x%02x  slave = 0x%02x   (a set bit = IRQ masked off)\n",
            imr1, imr2);
    kprintf("  unmasked IRQs:");
    for (int irq = 0; irq < 16; irq++) {
        uint8_t imr = irq < 8 ? imr1 : imr2;

        if (!(imr & (1u << (irq % 8)))) {
            kprintf(" %d", irq);
        }
    }
    kprintf("\n\n  vector name                      times fired since boot\n");
    for (int v = 0; v < 256; v++) {
        uint64_t n = interrupt_count((uint8_t)v);

        if (n) {
            kprintf("  %5d  %-24s %lu\n", v, interrupt_vector_name((uint8_t)v), n);
        }
    }
    kprintf("\n");
    learn_para("Vector 32 is the timer firing 100 times a second; 33 counts "
               "every scancode byte from the keyboard. The counters are bumped "
               "at the top of interrupt_dispatch() in interrupts.c.");
    LEARN_NEXT();

    learn_page("Hardware interrupts, the PIC and the timer");
    learn_heading("The PIT: a clock made of a divider");
    learn_para("The 8253/8254 Programmable Interval Timer has an oscillator "
               "running at 1,193,182 Hz and a counter we load with a divisor. "
               "Every time the counter reaches zero it raises IRQ 0. "
               "pit_init(100) loads 1193182 / 100 = 11931, giving about 100 "
               "interrupts per second: one tick every 10 ms. Everything "
               "time-based builds on that count - uptime, sleep, and later the "
               "scheduler's time slice.");
    kprintf("  divisor for %d Hz = %d  ->  real rate %d.%02d Hz\n\n", PIT_HZ,
            1193182 / PIT_HZ, 1193182 / (1193182 / PIT_HZ),
            (int)((1193182ull * 100 / (1193182 / PIT_HZ)) % 100));

    learn_live("checking the PIT against the battery-backed RTC");
    kprintf("  waiting for the RTC second to change (up to 3 s)...\n");
    if (wait_for_next_second() == 0) {
        uint64_t t0 = pit_get_ticks();

        kprintf("  counting PIT ticks during one full RTC second...\n");
        if (wait_for_next_second() == 0) {
            uint64_t ticks = pit_get_ticks() - t0;

            kprintf("  measured: %lu ticks in one RTC second (expected about %d)\n\n",
                    ticks, PIT_HZ);
        } else {
            kprintf("  RTC did not advance - skipping\n\n");
        }
    } else {
        kprintf("  RTC did not advance - skipping\n\n");
    }
    learn_para("Two independent clocks agree: that is how an OS calibrates "
               "time. The RTC knows the date but is slow to read; the PIT "
               "is fast but forgets everything at power-off.");
    learn_try("uptime    ticks    date    sleep 1000    (then 'learn irq' again)");
}
