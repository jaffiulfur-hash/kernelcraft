#ifndef ARCH_PIC_H
#define ARCH_PIC_H

#include <stdint.h>

#define PIC1        0x20
#define PIC2        0xA0
#define PIC1_CMD    PIC1
#define PIC1_DATA   (PIC1 + 1)
#define PIC2_CMD    PIC2
#define PIC2_DATA   (PIC2 + 1)

void pic_remap(int offset1, int offset2);
void pic_send_eoi(uint8_t irq);
void pic_mask_all(void);
void pic_unmask_irq(uint8_t irq);

#endif
