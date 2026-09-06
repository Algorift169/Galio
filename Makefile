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
           -Itools/compiler/include \
           -Iui/include \
           -Idrift/include

# Drift is built for both the hosted companion executable and the kernel shell.
DRIFT_SRCS = drift/main.c \
             drift/runtime.c \
             drift/platform_host.c \
             drift/token/token.c \
             drift/lexer/core/lexer.c \
             drift/lexer/keywords/logical_keywords.c \
             drift/lexer/keywords/identity_keywords.c \
             drift/lexer/comments/comments.c \
             drift/lexer/comments/executable_comments.c \
             drift/parser/core/parser.c \
             drift/parser/control_flow/if_parser.c \
             drift/parser/control_flow/repeat_parser.c \
             drift/parser/control_flow/for_parser.c \
             drift/parser/control_flow/while_parser.c \
             drift/parser/control_flow/each_parser.c \
             drift/parser/control_flow/unless_parser.c \
             drift/parser/control_flow/when_parser.c \
             drift/parser/functions/function_parser.c \
             drift/parser/arrays/array_parser.c \
             drift/parser/arrays/select_parser.c \
             drift/interpreter/core/value.c \
             drift/interpreter/core/environment.c \
             drift/interpreter/core/input.c \
             drift/interpreter/core/intptr.c \
             drift/interpreter/core/unless.c \
             drift/interpreter/core/when.c \
             drift/interpreter/functions/function.c \
             drift/interpreter/loop/repeat.c \
             drift/interpreter/loop/for.c \
             drift/interpreter/loop/while.c \
             drift/interpreter/loop/break.c \
             drift/interpreter/loop/continue.c \
             drift/interpreter/loop/each.c \
             drift/interpreter/arrays/array_value.c \
             drift/interpreter/arrays/array.c \
             drift/interpreter/operators/operator.c \
             drift/interpreter/operators/operator_identity.c \
             drift/interpreter/operators/operator_ternary.c
DRIFT_HEADERS = $(shell find drift/include -type f -name '*.h' -print)
DRIFT_BIN = $(BIN_DIR)/drift

CFLAGS = -m64 -mno-sse -mno-sse2 -mno-mmx -mno-3dnow -mno-avx -mcmodel=kernel -mno-red-zone -ffreestanding -fno-pie -no-pie -O2 -Wall -Wextra $(INCLUDES) -Wno-array-bounds -Wno-unused-function
ASFLAGS = -f elf64
LDFLAGS = -m elf_x86_64 -T kernel/arch/x86/boot/linker.ld
LDLIBS = $(shell $(CC) -m64 -print-file-name=libgcc.a)
USER_ELF_BASE = 0x40000000

# Source files
SRCS = kernel/kmain.c \
       kernel/auth.c \
       kernel/drift_platform.c \
       kernel/drift_compat.c \
       kernel/net/net.c \
       kernel/net/netdev.c \
       kernel/net/packet.c \
       kernel/net/util.c \
       kernel/net/ethernet.c \
       kernel/net/arp.c \
       kernel/net/ipv4.c \
       kernel/net/udp.c \
       kernel/net/dhcp.c \
       kernel/net/dns.c \
       kernel/net/tcp.c \
       kernel/net/socket.c \
       kernel/net/route.c \
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
       kernel/process/scheduler.c \
       kernel/cpu/cpu.c \
       kernel/drivers/msr.c \
       kernel/cpufreq/cpufreq.c \
       kernel/cpufreq/policy.c \
       kernel/cpufreq/governor.c \
       kernel/cpufreq/stats.c \
       kernel/drivers/cpufreq_x86.c \
       kernel/cpu/scheduler.c \
       kernel/process/elf.c \
       kernel/syscall/syscall.c \
       kernel/syscall/syscall_extra.c \
       kernel/process/signals.c \
       kernel/process/paths.c \
       kernel/fs/path.c \
       kernel/syscall/syscall_process.c \
       kernel/fs/vfs/vfs_core.c \
       kernel/fs/vfs/vfs_wrapper.c \
       kernel/fs/ext2/ext2.c \
       kernel/drivers/ata/ata.c \
       kernel/drivers/keyboard/keyboard.c \
       kernel/drivers/serial/serial.c \
       kernel/drivers/rtc/rtc.c \
       kernel/drivers/timer/pit.c \
       kernel/time.c \
       kernel/time/jiffies.c \
       kernel/time/clocksource.c \
       kernel/time/clockevents.c \
       kernel/time/ktimer.c \
       kernel/time/hrtimer.c \
       kernel/time/timekeeping.c \
       kernel/time/sched_clock.c \
       kernel/time/timeconv.c \
       kernel/time/sleep_timeout.c \
       kernel/power/power_main.c \
       kernel/power/power_suspend.c \
       kernel/power/power_process.c \
       kernel/power/power_console.c \
       kernel/power/power_qos.c \
       kernel/power/power_autosleep.c \
       kernel/power/power_wakelock.c \
       kernel/drivers/video/vga.c \
       kernel/drivers/video/fb.c \
       kernel/drivers/usb/usb.c \
       ui/src/display/display.c \
       ui/src/display/terminal_layer.c \
       ui/src/display/pk.c \
       ui/src/mouse/mouse.c \
       ui/src/mouse/cursor.c \
       ui/src/panel/panel.c \
       ui/src/panel/sysinfo.c \
       ui/src/panel/clock.c \
       ui/src/panel/date.c \
       ui/src/panel/fs_browser.c \
       ui/src/panel/launch_region.c \
       ui/src/buttons/galio.c \
       ui/src/buttons/gsh.c \
       kernel/pci/pci.c \
       kernel/drivers/net/e1000.c \
	kernel/dev/device.c \
	kernel/dev/device_manager.c \
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
       kernel/tests/cpufreq_test.c \
       init/init.c \
       tools/shell/shell.c \
       tools/shell/script.c \
       tools/shell/commands/cpufreq.c \
       tools/shell/options.c \
       kernel/shell/cmd_net.c \
       tools/shell/commands/file.c \
       tools/shell/commands/new.c \
       tools/shell/commands/show.c \
       tools/shell/commands/tree.c \
       tools/shell/commands/write.c \
       tools/shell/commands/chuser.c \
       tools/shell/commands/passwd.c \
       tools/shell/commands/recycle.c \
       tools/shell/commands/clean.c \
       tools/shell/commands/delete.c \
       tools/shell/commands/net.c \
       tools/shell/commands/ip.c \
       tools/shell/commands/pkg.c \
       tools/shell/commands/syscall.c \
       tools/shell/commands/wifi_list.c \
       tools/shell/commands/top.c \
       tools/shell/commands/spike.c \
          tools/compiler/src/gc.c \
          tools/compiler/src/lexer.c \
          tools/compiler/src/parser.c \
          tools/compiler/src/codegen.c \
          tools/compiler/src/assembler.c \
          tools/compiler/src/elf_writer.c \
          tools/compiler/src/libc_galio.c \
          tools/compiler/src/types.c \
          tools/shell/commands/gc.c \
	tools/shell/commands/where.c \
       tools/shell/editor/editor.c

# Link the real Drift runtime into the kernel so gsh uses the same grammar.
SRCS += $(filter-out drift/main.c drift/platform_host.c,$(DRIFT_SRCS))

# Object files
C_OBJS = $(patsubst %.c,$(OBJ_DIR)/%.o,$(SRCS))
# The legacy PIT assembly stub was written for 32-bit protected mode and is
# not compatible with x86-64 long mode. The timer functionality is already
# implemented in kernel/drivers/timer/pit.c using the 64-bit-safe ABI.
GAS_SRCS =
GAS_OBJS = $(patsubst %.s,$(OBJ_DIR)/%.o,$(GAS_SRCS))
ASM_OBJS = $(OBJ_DIR)/kernel/arch/x86/cpu/asm.o \
           $(OBJ_DIR)/kernel/arch/x86/cpu/isr_asm.o \
           $(OBJ_DIR)/kernel/arch/x86/boot/boot.o
EMBEDDED_OBJS = $(OBJ_DIR)/src/embedded_test.o \
                $(OBJ_DIR)/src/embedded_initrd.o
OBJS = $(C_OBJS) $(GAS_OBJS) $(ASM_OBJS) $(EMBEDDED_OBJS)

# Binary targets
TEST_ELF = $(BUILD_DIR)/test_elf.bin
INITRD_IMAGE = $(BUILD_DIR)/initrd.bin
DISK_IMAGE = $(BUILD_DIR)/disk.img
KERNEL_BIN = $(BIN_DIR)/galio.bin
KERNEL_ISO = $(BIN_DIR)/galio.iso

.PHONY: all clean run disk help drift-source drift-build

all: $(OBJS) $(KERNEL_BIN) $(KERNEL_ISO) $(DISK_IMAGE) $(DRIFT_BIN)
	@echo "Build complete!"
	@echo "Network stack: Ethernet, ARP, IPv4, ICMP"
	@echo "Wi-Fi: RTL8188EU driver with real 802.11 scanning"

drift-source:
	@test -n "$(DRIFT_SRCS)" || { echo "Drift sources missing"; exit 1; }
	@test -n "$(DRIFT_HEADERS)" || { echo "Drift headers missing"; exit 1; }
	@echo "Drift source tree: $$(printf '%s\n' $(DRIFT_SRCS) | wc -l) sources, $$(printf '%s\n' $(DRIFT_HEADERS) | wc -l) headers"

drift-build: $(DRIFT_BIN)

$(DRIFT_BIN): $(DRIFT_SRCS) $(DRIFT_HEADERS)
	@mkdir -p $(dir $@)
	$(CC) -std=c99 -Wall -Wextra -pedantic -Idrift/include $(DRIFT_SRCS) -o $@ -lm
	@echo "Drift interpreter built: $@"

# Explicit object rules for nested paths (must come before generic rule)
$(OBJ_DIR)/tools/shell/.created:
	@mkdir -p $(OBJ_DIR)/tools/shell $(OBJ_DIR)/tools/shell/commands $(OBJ_DIR)/tools/shell/editor

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

$(OBJ_DIR)/tools/shell/commands/%.o: tools/shell/commands/%.c | $(OBJ_DIR)/tools/shell/.created
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/tools/shell/editor/%.o: tools/shell/editor/%.c | $(OBJ_DIR)/tools/shell/.created
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/tools/shell/%.o: tools/shell/%.c | $(OBJ_DIR)/tools/shell/.created
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/drift/%.o: drift/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -include drift/kernel_compat.h -c $< -o $@

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

$(OBJ_DIR)/kernel/drivers/net/%.o: kernel/drivers/net/%.s
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
	$(CC) -O2 -Wall -Wextra -o $@ $<

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
$(KERNEL_ISO): $(KERNEL_BIN) $(INITRD_IMAGE) $(DRIFT_BIN)
	@command -v grub-mkrescue >/dev/null 2>&1 || { echo "Error: grub-mkrescue not found"; exit 1; }
	@rm -rf $(ISO_DIR)
	@mkdir -p $(ISO_DIR)/boot/grub
	@cp $(KERNEL_BIN) $(ISO_DIR)/boot/galio.bin
	@cp $(INITRD_IMAGE) $(ISO_DIR)/boot/initrd.bin
	@cp $(DRIFT_BIN) $(ISO_DIR)/boot/drift
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
	cd $(BUILD_DIR) && objcopy -I binary -O elf64-x86-64 -B i386:x86-64 $(patsubst $(BUILD_DIR)/%,%,$<) $(patsubst $(BUILD_DIR)/%,%,$@)

$(OBJ_DIR)/src/embedded_initrd.o: $(INITRD_IMAGE)
	@mkdir -p $(dir $@)
	cd $(BUILD_DIR) && objcopy -I binary -O elf64-x86-64 -B i386:x86-64 $(patsubst $(BUILD_DIR)/%,%,$<) $(patsubst $(BUILD_DIR)/%,%,$@)

$(OBJ_DIR)/test/test_elf.o: test/test_elf.c
	@mkdir -p $(dir $@)
	$(CC) -m32 -march=i686 -O2 -Wall -Wextra -c $< -o $@

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
	qemu-system-x86_64 -cdrom $(KERNEL_ISO) -hda $(DISK_IMAGE) \
		-netdev user,id=net0,restrict=off,hostfwd=udp::7777-:7777,hostfwd=tcp::8888-:8888 \
		-device e1000,netdev=net0 \
		-usb -device usb-host,hostbus=1,hostaddr=2 \
		-serial stdio \
		-monitor none \
		-no-reboot \
		-cpu qemu64 \
		-m 128M

# Run with USB passthrough for real Wi-Fi dongle
run-usb: $(KERNEL_ISO) $(DISK_IMAGE)
	qemu-system-x86_64 -cdrom $(KERNEL_ISO) -hda $(DISK_IMAGE) \
              -netdev user,id=net0,restrict=off \
              -device e1000,netdev=net0 \
		-usb -device usb-host,hostbus=1,hostaddr=2 \
		-serial stdio \
		-monitor none \
		-no-reboot \
		-cpu qemu64 \
		-m 128M

# Debug build with symbols
debug: CFLAGS += -g -O0
debug: all
	@echo "Debug build complete (use 'make run-debug' to run)"

run-debug: $(KERNEL_ISO) $(DISK_IMAGE)
	qemu-system-x86_64 -cdrom $(KERNEL_ISO) -hda $(DISK_IMAGE) \
              -netdev user,id=net0,restrict=off \
              -device e1000,netdev=net0 \
		-s -S \
		-serial stdio \
		-monitor stdio \
		-no-reboot \
		-cpu qemu64 \
		-m 128M

# Help
help:
	@echo "Galio Kernel Build System"
	@echo "=========================="
	@echo ""
	@echo "Build targets:"
	@echo "  make all       - Build everything (kernel, ISO, disk image)"
	@echo "  make drift-build - Build the Drift interpreter executable"
	@echo "  make drift-source - Validate the root Drift source tree"
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
	@echo "  grub-mkrescue, qemu-system-x86_64, mtools, xorriso"
