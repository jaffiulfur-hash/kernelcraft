CC      = gcc
LD      = ld
NASM    = nasm
QEMU    = qemu-system-x86_64

CFLAGS  = -m64 -ffreestanding -fno-stack-protector -fno-pic -mno-red-zone -Wall -Wextra -std=c11 -Iinclude
LDFLAGS = -nostdlib -z max-page-size=0x1000 -T linker.ld

BUILD_DIR = build
ISO_DIR   = $(BUILD_DIR)/iso
KERNEL    = $(BUILD_DIR)/kernel.elf
ISO_IMAGE = $(BUILD_DIR)/kernelcraft.iso

C_OBJS = \
	$(BUILD_DIR)/kmain.o \
	$(BUILD_DIR)/panic.o \
	$(BUILD_DIR)/vga.o \
	$(BUILD_DIR)/shell.o \
	$(BUILD_DIR)/gdt.o \
	$(BUILD_DIR)/idt.o \
	$(BUILD_DIR)/interrupts.o \
	$(BUILD_DIR)/debug.o \
	$(BUILD_DIR)/serial.o \
	$(BUILD_DIR)/log.o \
	$(BUILD_DIR)/io.o \
	$(BUILD_DIR)/pic.o \
	$(BUILD_DIR)/pit.o \
	$(BUILD_DIR)/keyboard.o \
	$(BUILD_DIR)/multiboot2.o \
	$(BUILD_DIR)/pmm.o

ASM_OBJS = \
	$(BUILD_DIR)/boot.o \
	$(BUILD_DIR)/gdt_flush.o \
	$(BUILD_DIR)/isr.o \
	$(BUILD_DIR)/irq.o

all: $(ISO_IMAGE)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/kmain.o: src/kmain.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/panic.o: src/panic.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/vga.o: src/vga.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/shell.o: src/shell.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/gdt.o: src/arch/x86_64/gdt.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/idt.o: src/arch/x86_64/idt.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/interrupts.o: src/arch/x86_64/interrupts.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/debug.o: src/arch/x86_64/debug.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/serial.o: src/arch/x86_64/serial.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/log.o: src/arch/x86_64/log.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/io.o: src/arch/x86_64/io.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/pic.o: src/arch/x86_64/pic.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/pit.o: src/arch/x86_64/pit.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/keyboard.o: src/arch/x86_64/keyboard.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/boot.o: src/arch/x86_64/boot.asm | $(BUILD_DIR)
	$(NASM) -f elf64 $< -o $@

$(BUILD_DIR)/gdt_flush.o: src/arch/x86_64/gdt_flush.asm | $(BUILD_DIR)
	$(NASM) -f elf64 $< -o $@

$(BUILD_DIR)/isr.o: src/arch/x86_64/isr.asm | $(BUILD_DIR)
	$(NASM) -f elf64 $< -o $@

$(BUILD_DIR)/irq.o: src/arch/x86_64/irq.asm | $(BUILD_DIR)
	$(NASM) -f elf64 $< -o $@
	
$(BUILD_DIR)/multiboot2.o: src/boot/multiboot2.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/pmm.o: src/mm/pmm.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@	

$(KERNEL): $(C_OBJS) $(ASM_OBJS)
	$(LD) $(LDFLAGS) -o $@ $^

$(ISO_IMAGE): $(KERNEL)
	mkdir -p $(ISO_DIR)/boot/grub
	cp $(KERNEL) $(ISO_DIR)/boot/kernel.elf
	cp iso/boot/grub/grub.cfg $(ISO_DIR)/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO_IMAGE) $(ISO_DIR)

run: $(ISO_IMAGE)
	$(QEMU) -cdrom $(ISO_IMAGE) -serial stdio

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all run clean
