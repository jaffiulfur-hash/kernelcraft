#ifndef ARCH_GDT_H
#define ARCH_GDT_H

#include <stdint.h>

/*
 * Segment selectors. The low two bits of a selector are the requested
 * privilege level (RPL), so the user selectors end in 3.
 */
#define GDT_KERNEL_CODE 0x08
#define GDT_KERNEL_DATA 0x10
#define GDT_USER_DATA   0x1B      /* index 3, ring 3 */
#define GDT_USER_CODE   0x23      /* index 4, ring 3 */
#define GDT_TSS         0x28      /* index 5 (a 16-byte descriptor: 5 and 6) */

void gdt_init(void);

/*
 * The TSS tells the CPU which stack to switch to when an interrupt or system
 * call arrives from ring 3. The scheduler points it at the running user
 * thread's kernel stack on every context switch.
 */
void gdt_set_kernel_stack(uint64_t rsp0);
uint64_t gdt_get_kernel_stack(void);
uint64_t gdt_tss_address(void);

#endif
