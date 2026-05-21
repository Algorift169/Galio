# Galio Kernel Makefile

BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/obj
BIN_DIR = $(BUILD_DIR)
ISO_DIR = $(BUILD_DIR)/iso

CC = gcc
AS = nasm
LD = ld

# Include paths
INCLUDES = -Iinclude \
           -Iinclude/arch/x86 \
           -Iinclude/drivers \
           -Iinclude/fs \
           -Iinclude/lib \
           -Iinclude/mm \
           -Iinclude/process \
           -Iinclude/syscall \
           -Itools/shell/include \
           -Itools/shell/commands \
           -Itools/shell/editor

CFLAGS = -m32 -ffreestanding -O2 -Wall -Wextra $(INCLUDES) -Wno-array-bounds
ASFLAGS = -f elf32
LDFLAGS = -m elf_i386 -T kernel/arch/x86/boot/linker.ld
USER_ELF_BASE = 0x40000000

# Source files
SRCS = kernel/kmain.c \
       kernel/auth.c \
       kernel/lib/kernel.c \
       kernel/lib/kprintf.c \
       kernel/lib/string.c \
       kernel/lib/mem_test.c \
       kernel/arch/x86/cpu/gdt.c \
       kernel/arch/x86/cpu/tss.c \
       kernel/arch/x86/cpu/idt.c \
       kernel/arch/x86/cpu/irq.c \
       kernel/arch/x86/cpu/isr.c \
       kernel/mm/paging.c \
       kernel/mm/pmem.c \
       kernel/mm/heap.c \
       kernel/mm/memory.c \
       kernel/process/process.c \
       kernel/process/scheduler.c \
       kernel/process/elf.c \
       kernel/syscall/syscall.c \
       kernel/syscall/syscall_extra.c \
       kernel/process/signals.c \
       kernel/process/paths.c \
       kernel/fs/path.c \
       kernel/fs/vfs/vfs_core.c \
       kernel/fs/vfs/vfs_wrapper.c \
       kernel/fs/ext2/ext2.c \
       kernel/drivers/ata/ata.c \
       kernel/drivers/keyboard/keyboard.c \
       kernel/drivers/serial/serial.c \
       kernel/drivers/timer/pit.c \
       kernel/drivers/video/vga.c \
       init/init.c \
       tools/shell/shell.c \
       tools/shell/commands/file.c \
       tools/shell/commands/new.c \
       tools/shell/commands/show.c \
       tools/shell/commands/write.c \
       tools/shell/commands/recycle.c \
       tools/shell/commands/clean.c \
       tools/shell/commands/delete.c \
       tools/shell/editor/editor.c

# Object files
C_OBJS = $(patsubst %.c,$(OBJ_DIR)/%.o,$(SRCS))
ASM_OBJS = $(OBJ_DIR)/kernel/arch/x86/cpu/asm.o \
           $(OBJ_DIR)/kernel/arch/x86/cpu/isr_asm.o \
           $(OBJ_DIR)/kernel/arch/x86/boot/boot.o
EMBEDDED_OBJS = $(OBJ_DIR)/src/embedded_test.o \
                $(OBJ_DIR)/src/embedded_initrd.o
OBJS = $(C_OBJS) $(ASM_OBJS) $(EMBEDDED_OBJS)

# Binary targets
TEST_ELF = $(BUILD_DIR)/test_elf.bin
INITRD_IMAGE = $(BUILD_DIR)/initrd.bin
DISK_IMAGE = $(BUILD_DIR)/disk.img
KERNEL_BIN = $(BIN_DIR)/galio.bin
KERNEL_ISO = $(BIN_DIR)/galio.iso

.PHONY: all clean run disk help

all: $(OBJS) $(KERNEL_BIN) $(KERNEL_ISO) $(DISK_IMAGE)
	@echo "Build complete!"

# Compile C files
$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Assembly files
$(OBJ_DIR)/kernel/arch/x86/cpu/asm.o: kernel/arch/x86/cpu/asm.s
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

$(OBJ_DIR)/kernel/arch/x86/cpu/isr_asm.o: kernel/arch/x86/cpu/isr_asm.s
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

$(OBJ_DIR)/kernel/arch/x86/boot/boot.o: kernel/arch/x86/boot/boot.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Link kernel
$(KERNEL_BIN): $(OBJS)
	@mkdir -p $(dir $@)
	$(LD) $(LDFLAGS) -o $@ $(OBJS)
	@echo "Kernel linked: $@"

# mkiofs tool
MKIOFS = $(OBJ_DIR)/tools/mkiofs/mkiofs
$(MKIOFS): tools/mkiofs/mkiofs.c
	@mkdir -p $(dir $@)
	gcc -m32 -o $@ $<

# Initrd image
$(INITRD_IMAGE): $(MKIOFS)
	@mkdir -p $(dir $@)
	./$(MKIOFS) $@

# Disk image
$(DISK_IMAGE):
	@mkdir -p $(dir $@)
	@echo "Creating 64MB disk image..."
	@dd if=/dev/zero of=$(DISK_IMAGE) bs=1M count=64 2>/dev/null
	@mkfs.ext2 -F -q $(DISK_IMAGE) 2>/dev/null || true
	@echo "Disk image created"

# ISO image
$(KERNEL_ISO): $(KERNEL_BIN) $(INITRD_IMAGE)
	@command -v grub-mkrescue >/dev/null 2>&1 || { echo "Error: grub-mkrescue not found"; exit 1; }
	@rm -rf $(ISO_DIR)
	@mkdir -p $(ISO_DIR)/boot/grub
	@cp $(KERNEL_BIN) $(ISO_DIR)/boot/galio.bin
	@cp $(INITRD_IMAGE) $(ISO_DIR)/boot/initrd.bin
	@printf '%s\n' 'set timeout=0' 'set default=0' '' \
		'menuentry "Galio Kernel" {' \
		'  multiboot /boot/galio.bin' \
		'  boot' \
		'}' > $(ISO_DIR)/boot/grub/grub.cfg
	@grub-mkrescue -o $(KERNEL_ISO) $(ISO_DIR) 2>/dev/null
	@echo "ISO created"

# Embedded objects
$(OBJ_DIR)/src/embedded_test.o: $(TEST_ELF)
	@mkdir -p $(dir $@)
	cd $(BUILD_DIR) && objcopy -I binary -O elf32-i386 -B i386 $(notdir $<) $(patsubst $(BUILD_DIR)/%,%,$@)

$(OBJ_DIR)/src/embedded_initrd.o: $(INITRD_IMAGE)
	@mkdir -p $(dir $@)
	cd $(BUILD_DIR) && objcopy -I binary -O elf32-i386 -B i386 $(notdir $<) $(patsubst $(BUILD_DIR)/%,%,$@)

$(OBJ_DIR)/test/test_elf.o: test/test_elf.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/test_elf.elf: $(OBJ_DIR)/test/test_elf.o
	@mkdir -p $(dir $@)
	$(LD) -m elf_i386 -Ttext=$(USER_ELF_BASE) --entry=_start $< -o $@

$(TEST_ELF): $(BUILD_DIR)/test_elf.elf
	@mkdir -p $(dir $@)
	cp $< $@

# Clean
clean:
	@echo "Cleaning build files..."
	rm -rf $(BUILD_DIR)
	@echo "Clean complete"

# Run in QEMU
run: $(KERNEL_ISO) $(DISK_IMAGE)
	./scripts/run.sh

# Help
help:
	@echo "Galio Kernel Build System"
	@echo "  make all    - Build everything"
	@echo "  make clean  - Clean build files"
	@echo "  make run    - Run in QEMU"