# Galio Kernel Makefile

BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/obj
BIN_DIR = $(BUILD_DIR)/bin
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
           -Iinclude/net \
           -Iinclude/process \
           -Iinclude/syscall \
           -Itools/shell/include \
           -Itools/shell/commands \
           -Itools/shell/editor \
           -Iui/include

CFLAGS = -m32 -ffreestanding -O2 -Wall -Wextra $(INCLUDES) -Wno-array-bounds -Wno-unused-function
ASFLAGS = -f elf32
LDFLAGS = -m elf_i386 -T kernel/arch/x86/boot/linker.ld
LDLIBS = $(shell $(CC) -m32 -print-file-name=libgcc.a)
USER_ELF_BASE = 0x40000000

# Source files
SRCS = kernel/kmain.c \
       kernel/auth.c \
       kernel/net/net.c \
       kernel/net/netdev.c \
       kernel/net/packet.c \
       kernel/net/util.c \
       kernel/net/ethernet.c \
       kernel/net/arp.c \
       kernel/net/ipv4.c \
       kernel/net/udp.c \
       kernel/net/tcp.c \
       kernel/net/http.c \
       kernel/net/80211.c \
       kernel/lib/kernel.c \
       kernel/lib/kprintf.c \
       kernel/lib/string.c \
       kernel/lib/mem_test.c \
       kernel/arch/x86/cpu/gdt.c \
       kernel/arch/x86/cpu/tss.c \
       kernel/arch/x86/cpu/idt.c \
	kernel/process/spinlock.c \
	kernel/security/security.c \
       kernel/arch/x86/cpu/irq.c \
       kernel/arch/x86/cpu/isr.c \
       kernel/mm/paging.c \
       kernel/mm/pmem.c \
       kernel/mm/heap.c \
       kernel/mm/memory.c \
       kernel/mm/dma.c \
       kernel/process/process.c \
       kernel/cpu/cpu.c \
       kernel/cpu/scheduler.c \
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
       kernel/drivers/rtc/rtc.c \
	kernel/time.c \
       kernel/drivers/video/vga.c \
       kernel/drivers/usb/usb.c \
       ui/src/display/display.c \
       ui/src/display/pk.c \
       ui/src/mouse/mouse.c \
       ui/src/mouse/cursor.c \
       ui/src/panel/panel.c \
       ui/src/panel/clock.c \
       ui/src/panel/date.c \
       ui/src/buttons/galio.c \
       ui/src/buttons/gsh.c \
       kernel/pci/pci.c \
       kernel/drivers/net/e1000.c \
       kernel/drivers/net/wifi.c \
       kernel/drivers/net/rtl8188eu.c \
       kernel/tests/run_tests.c \
       kernel/tests/scheduler_test.c \
       kernel/tests/cpu_scheduler_test.c \
       kernel/tests/paging_test.c \
       kernel/tests/vfs_test.c \
       kernel/tests/signal_test.c \
       kernel/tests/heap_test.c \
       kernel/tests/security_test.c \
       init/init.c \
       tools/shell/shell.c \
       kernel/shell/cmd_net.c \
       tools/shell/commands/file.c \
       tools/shell/commands/new.c \
       tools/shell/commands/show.c \
       tools/shell/commands/tree.c \
       tools/shell/commands/write.c \
       tools/shell/commands/recycle.c \
       tools/shell/commands/clean.c \
       tools/shell/commands/delete.c \
       tools/shell/commands/net.c \
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
	@echo "Network stack: Ethernet, ARP, IPv4, ICMP"
	@echo "Wi-Fi: RTL8188EU driver with real 802.11 scanning"

# Explicit object rules for nested paths (must come before generic rule)
$(OBJ_DIR)/kernel/drivers/net/%.o: kernel/drivers/net/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/kernel/drivers/usb/%.o: kernel/drivers/usb/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/kernel/shell/%.o: kernel/shell/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/kernel/net/%.o: kernel/net/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/kernel/pci/%.o: kernel/pci/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/kernel/mm/%.o: kernel/mm/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/tools/shell/commands/%.o: tools/shell/commands/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/tools/shell/editor/%.o: tools/shell/editor/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Generic compile C files rule
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
	$(LD) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)
	@echo "Kernel linked: $@"

# mkiofs tool
MKIOFS = $(BIN_DIR)/mkiofs
$(MKIOFS): tools/mkiofs/mkiofs.c
	@mkdir -p $(dir $@)
	$(CC) -m32 -o $@ $<

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
	cd $(BUILD_DIR) && objcopy -I binary -O elf32-i386 -B i386 $(patsubst $(BUILD_DIR)/%,%,$<) $(patsubst $(BUILD_DIR)/%,%,$@)

$(OBJ_DIR)/src/embedded_initrd.o: $(INITRD_IMAGE)
	@mkdir -p $(dir $@)
	cd $(BUILD_DIR) && objcopy -I binary -O elf32-i386 -B i386 $(patsubst $(BUILD_DIR)/%,%,$<) $(patsubst $(BUILD_DIR)/%,%,$@)

$(OBJ_DIR)/test/test_elf.o: test/test_elf.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN_DIR)/test_elf.elf: $(OBJ_DIR)/test/test_elf.o
	@mkdir -p $(dir $@)
	$(LD) -m elf_i386 -Ttext=$(USER_ELF_BASE) --entry=_start $< -o $@

$(TEST_ELF): $(BIN_DIR)/test_elf.elf
	@mkdir -p $(dir $@)
	cp $< $@

# Clean
clean:
	@echo "Cleaning build files..."
	rm -rf $(BUILD_DIR)
	@echo "Clean complete"

# Run in QEMU with network support
run: $(KERNEL_ISO) $(DISK_IMAGE)
	./scripts/run.sh

# Run with network emulation (e1000 + USB for Wi-Fi testing)
run-net: $(KERNEL_ISO) $(DISK_IMAGE)
	qemu-system-i386 -cdrom $(KERNEL_ISO) -hda $(DISK_IMAGE) \
		-netdev user,id=net0,hostfwd=udp::7777-:7777,hostfwd=tcp::8888-:8888 \
		-device e1000,netdev=net0 \
		-usb -device usb-host,hostbus=1,hostaddr=2 \
		-serial stdio \
		-monitor none \
		-no-reboot \
		-cpu qemu32 \
		-m 128M

# Run with USB passthrough for real Wi-Fi dongle
run-usb: $(KERNEL_ISO) $(DISK_IMAGE)
	qemu-system-i386 -cdrom $(KERNEL_ISO) -hda $(DISK_IMAGE) \
		-usb -device usb-host,hostbus=1,hostaddr=2 \
		-serial stdio \
		-monitor none \
		-no-reboot \
		-cpu qemu32 \
		-m 128M

# Debug build with symbols
debug: CFLAGS += -g -O0
debug: all
	@echo "Debug build complete (use 'make run-debug' to run)"

run-debug: $(KERNEL_ISO) $(DISK_IMAGE)
	qemu-system-i386 -cdrom $(KERNEL_ISO) -hda $(DISK_IMAGE) \
		-s -S \
		-serial stdio \
		-monitor stdio \
		-no-reboot \
		-cpu qemu32 \
		-m 128M

# Help
help:
	@echo "Galio Kernel Build System"
	@echo "=========================="
	@echo ""
	@echo "Build targets:"
	@echo "  make all       - Build everything (kernel, ISO, disk image)"
	@echo "  make clean     - Clean build files"
	@echo "  make debug     - Build with debug symbols"
	@echo ""
	@echo "Run targets:"
	@echo "  make run       - Run in QEMU (normal mode)"
	@echo "  make run-net   - Run with e1000 NIC + USB for Wi-Fi testing"
	@echo "  make run-usb   - Run with USB passthrough for real Wi-Fi dongle"
	@echo "  make run-debug - Run with GDB debugging server"
	@echo ""
	@echo "Network features:"
	@echo "  - e1000 Ethernet driver (real)"
	@echo "  - RTL8188EU Wi-Fi driver (real)"
	@echo "  - ARP, IPv4, ICMP protocol stack"
	@echo "  - 802.11 beacon/probe response parsing"
	@echo "  - Real Wi-Fi scanning with channel hopping"
	@echo ""
	@echo "Prerequisites:"
	@echo "  grub-mkrescue, qemu-system-i386, mtools, xorriso"
