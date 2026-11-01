#ifndef KERNEL_H
#define KERNEL_H

#include <stdint.h>

#define KERNELCRAFT_NAME        "KernelCraft OS"
#define KERNELCRAFT_STAGE       "5.0"
#define KERNELCRAFT_STAGE_TITLE "the teaching kernel: interactive lessons"

void kmain(uint32_t mb_magic, uint32_t mb_info_addr);

#endif
