CC      = gcc
LD      = ld
NASM    = nasm
QEMU    = qemu-system-x86_64

# Output directory. `make test` overrides this to keep its -DKC_SELFTEST build
# separate from the normal one.
BUILD_DIR ?= build

# Extra compiler flags, e.g. `make EXTRA_CFLAGS=-O2`.
EXTRA_CFLAGS ?=

# -mgeneral-regs-only keeps GCC away from SSE/x87 registers, which the kernel
# has not enabled. -MMD -MP write .d files so editing a header rebuilds
# everything that includes it.
CFLAGS  = -m64 -ffreestanding -fno-stack-protector -fno-pic -mno-red-zone \
          -mgeneral-regs-only -Wall -Wextra -std=c11 -Iinclude -MMD -MP \
          $(EXTRA_CFLAGS)
LDFLAGS = -nostdlib -z max-page-size=0x1000 -z noexecstack -T linker.ld

# 128 MiB matches PMM_MAX_MEMORY_BYTES; more RAM is simply left untracked.
QEMU_FLAGS = -m 128M

# Every .c and .asm file under src/ is picked up automatically. Objects mirror
# the source tree (src/mm/pmm.c -> $(BUILD_DIR)/mm/pmm.o), so a .c and .asm
# file must not share a name within one directory.
SRC_C   := $(shell find src -name '*.c' | sort)
SRC_ASM := $(shell find src -name '*.asm' | sort)
OBJS    := $(SRC_C:src/%.c=$(BUILD_DIR)/%.o) $(SRC_ASM:src/%.asm=$(BUILD_DIR)/%.o)
DEPS    := $(OBJS:.o=.d)

ISO_DIR   = $(BUILD_DIR)/iso
KERNEL    = $(BUILD_DIR)/kernel.elf
ISO_IMAGE = $(BUILD_DIR)/kernelcraft.iso

all: $(ISO_IMAGE)

$(BUILD_DIR)/%.o: src/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: src/%.asm
	@mkdir -p $(@D)
	$(NASM) -f elf64 $< -o $@

$(KERNEL): $(OBJS) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

$(ISO_IMAGE): $(KERNEL) iso/boot/grub/grub.cfg
	mkdir -p $(ISO_DIR)/boot/grub
	cp $(KERNEL) $(ISO_DIR)/boot/kernel.elf
	cp iso/boot/grub/grub.cfg $(ISO_DIR)/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO_IMAGE) $(ISO_DIR)

# Boot in a QEMU window; serial output also goes to this terminal.
run: $(ISO_IMAGE)
	$(QEMU) $(QEMU_FLAGS) -cdrom $(ISO_IMAGE) -serial stdio

# Start paused with a GDB stub on :1234. Then: gdb build/kernel.elf
#   (gdb) target remote :1234
debug: $(ISO_IMAGE)
	$(QEMU) $(QEMU_FLAGS) -cdrom $(ISO_IMAGE) -serial stdio -s -S

# Boot a -DKC_SELFTEST build headless. The kernel runs its self-tests and
# leaves through QEMU's isa-debug-exit device: exit status 33 means success.
TEST_BUILD_DIR = $(BUILD_DIR)/test
TEST_TIMEOUT   = 60

test:
	$(MAKE) --no-print-directory BUILD_DIR=$(TEST_BUILD_DIR) \
		EXTRA_CFLAGS="$(EXTRA_CFLAGS) -DKC_SELFTEST" $(TEST_BUILD_DIR)/kernelcraft.iso
	timeout $(TEST_TIMEOUT) $(QEMU) $(QEMU_FLAGS) -cdrom $(TEST_BUILD_DIR)/kernelcraft.iso \
		-display none -serial stdio -no-reboot \
		-device isa-debug-exit,iobase=0xf4,iosize=0x04; \
	status=$$?; \
	if [ $$status -eq 33 ]; then \
		echo "make test: PASS"; \
	else \
		echo "make test: FAIL (qemu exit status $$status; 33 = pass, 124 = timeout)"; \
		exit 1; \
	fi

clean:
	rm -rf $(BUILD_DIR)

-include $(DEPS)

.PHONY: all run debug test clean
