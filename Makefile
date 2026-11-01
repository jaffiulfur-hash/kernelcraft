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

# ---- user programs ---------------------------------------------------------
# Every user/prog_<name>.c becomes a real ELF executable (linked at USER_BASE
# by user/user.ld) that is embedded in the kernel image and loaded at run time
# by `run <name>`. No per-program rules: just add a file.
USER_CFLAGS = -m64 -ffreestanding -fno-stack-protector -fno-pic -fno-pie -mno-red-zone \
              -mgeneral-regs-only -Wall -Wextra -std=c11 -O1 -Iuser -Iinclude -MMD -MP
USER_PROGS  := $(patsubst user/prog_%.c,%,$(sort $(wildcard user/prog_*.c)))
USER_ELFS   := $(USER_PROGS:%=$(BUILD_DIR)/user/%.elf)
USER_COMMON := $(BUILD_DIR)/user/crt0.o $(BUILD_DIR)/user/ulib.o
USER_BLOB   := $(BUILD_DIR)/user/blob.o
OBJS        += $(USER_BLOB)
DEPS        += $(wildcard $(BUILD_DIR)/user/*.d)

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

$(BUILD_DIR)/user/%.o: user/%.c
	@mkdir -p $(@D)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(BUILD_DIR)/user/crt0.o: user/crt0.asm
	@mkdir -p $(@D)
	$(NASM) -f elf64 $< -o $@

$(BUILD_DIR)/user/%.elf: $(USER_COMMON) $(BUILD_DIR)/user/prog_%.o user/user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -z noexecstack -T user/user.ld -o $@ \
		$(USER_COMMON) $(BUILD_DIR)/user/prog_$*.o

$(BUILD_DIR)/user/blob.asm: $(USER_ELFS) tools/mkblob.sh
	@mkdir -p $(@D)
	sh tools/mkblob.sh $@ $(USER_ELFS)

$(BUILD_DIR)/user/blob.o: $(BUILD_DIR)/user/blob.asm
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
TEST_TIMEOUT   = 120

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
