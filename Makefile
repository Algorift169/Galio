# Galio Kernel Makefile

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

# Source files
SRCS = kernel/kmain.c \
       kernel/auth.c \
       kernel/lib/kernel.c \
       kernel/lib/kprintf.c \
       kernel/lib/string.c \
       kernel/lib/mem_test.c \
       kernel/arch/x86/cpu/gdt.c \
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
OBJS = src/embedded_test.o src/embedded_initrd.o \
       kernel/kmain.o kernel/auth.o \
       kernel/lib/kernel.o kernel/lib/kprintf.o kernel/lib/string.o kernel/lib/mem_test.o \
       kernel/arch/x86/cpu/gdt.o kernel/arch/x86/cpu/idt.o \
       kernel/arch/x86/cpu/irq.o kernel/arch/x86/cpu/isr.o \
       kernel/mm/paging.o kernel/mm/pmem.o kernel/mm/heap.o kernel/mm/memory.o \
       kernel/process/process.o kernel/process/scheduler.o kernel/process/elf.o \
       kernel/syscall/syscall.o kernel/syscall/syscall_extra.o \
       kernel/process/signals.o kernel/process/paths.o kernel/fs/path.o \
       kernel/fs/vfs/vfs_core.o kernel/fs/vfs/vfs_wrapper.o kernel/fs/ext2/ext2.o \
       kernel/drivers/ata/ata.o kernel/drivers/keyboard/keyboard.o \
       kernel/drivers/serial/serial.o kernel/drivers/timer/pit.o kernel/drivers/video/vga.o \
       init/init.o \
       tools/shell/shell.o \
       tools/shell/commands/file.o tools/shell/commands/new.o tools/shell/commands/show.o \
       tools/shell/commands/write.o tools/shell/commands/recycle.o tools/shell/commands/clean.o \
       tools/shell/commands/delete.o tools/shell/editor/editor.o \
       kernel/arch/x86/cpu/asm.o kernel/arch/x86/cpu/isr_asm.o kernel/arch/x86/boot/boot.o

# Binary targets
TEST_ELF = test_elf.bin
INITRD_IMAGE = initrd.bin
DISK_IMAGE = disk.img
KERNEL_BIN = galio.bin
KERNEL_ISO = galio.iso

.PHONY: all clean run disk help

all: $(OBJS) $(KERNEL_BIN) $(KERNEL_ISO) $(DISK_IMAGE)
	@echo "Build complete!"

# Compile C files
%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Assembly files
kernel/arch/x86/cpu/asm.o: kernel/arch/x86/cpu/asm.s
	@mkdir -p kernel/arch/x86/cpu
	$(AS) $(ASFLAGS) $< -o $@

kernel/arch/x86/cpu/isr_asm.o: kernel/arch/x86/cpu/isr_asm.s
	@mkdir -p kernel/arch/x86/cpu
	$(AS) $(ASFLAGS) $< -o $@

kernel/arch/x86/boot/boot.o: kernel/arch/x86/boot/boot.S
	@mkdir -p kernel/arch/x86/boot
	$(CC) $(CFLAGS) -c $< -o $@

# Link kernel
$(KERNEL_BIN): $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $(OBJS)
	@echo "Kernel linked: $@"

# mkiofs tool
MKIOFS = tools/mkiofs/mkiofs
$(MKIOFS): tools/mkiofs/mkiofs.c
	@mkdir -p tools/mkiofs
	gcc -o $@ $<

# Initrd image
$(INITRD_IMAGE): $(MKIOFS)
	./$(MKIOFS) $@

# Disk image
$(DISK_IMAGE):
	@echo "Creating 64MB disk image..."
	@dd if=/dev/zero of=$(DISK_IMAGE) bs=1M count=64 2>/dev/null
	@mkfs.ext2 -F -q $(DISK_IMAGE) 2>/dev/null || true
	@echo "Disk image created"

# ISO image
$(KERNEL_ISO): $(KERNEL_BIN)
	@command -v grub-mkrescue >/dev/null 2>&1 || { echo "Error: grub-mkrescue not found"; exit 1; }
	@rm -rf iso
	@mkdir -p iso/boot/grub
	@cp $(KERNEL_BIN) iso/boot/galio.bin
	@printf '%s\n' 'set timeout=0' 'set default=0' '' \
		'menuentry "Galio Kernel" {' \
		'  multiboot /boot/galio.bin' \
		'  boot' \
		'}' > iso/boot/grub/grub.cfg
	@grub-mkrescue -o $(KERNEL_ISO) iso 2>/dev/null
	@echo "ISO created"

# Embedded objects
src/embedded_test.o: test_elf.bin
	@mkdir -p src
	objcopy -I binary -O elf32-i386 -B i386 $< $@

src/embedded_initrd.o: $(INITRD_IMAGE)
	@mkdir -p src
	objcopy -I binary -O elf32-i386 -B i386 $< $@

test_elf.bin: test/test_elf.c
	gcc -m32 -ffreestanding -nostdlib -Wl,--entry=_start -Wl,-Ttext=0x10000 $< -o $@

# Clean
clean:
	@echo "Cleaning build files..."
	rm -f $(KERNEL_BIN) $(KERNEL_ISO) $(DISK_IMAGE)
	rm -f $(MKIOFS) $(INITRD_IMAGE) $(TEST_ELF)
	rm -f $(OBJS)
	rm -rf iso
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