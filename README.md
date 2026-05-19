# Galio — 32-bit Kernel 

**Galio** is a comprehensive 32-bit kernel designed  for a system. It provides a full protected-mode environment with memory management, interrupt handling, device drivers, and basic process support, enabling the development of higher-level OS components.

---

### Features Implemented

**Boot and Initialization**
- Multiboot v1 compliant header for GRUB loading
- Assembly boot stub with stack setup and early serial output
- Complete kernel initialization sequence

**Memory Management**
- Physical Memory Manager (PMM) with bitmap allocation
- Virtual memory paging with 4MB identity mapping
- Kernel heap allocator
- Multiboot memory map parsing

**CPU and Interrupts**
- GDT setup (null, code, data segments)
- IDT with ISR and IRQ handlers
- PIC remapping and interrupt management
- CPU exception handling with panic on faults

**Device Drivers**
- VGA text output with scrolling and cursor management
- Serial port driver (COM1) for debugging output
- PIT (Programmable Interval Timer) for system timing (100 Hz)
- PS/2 Keyboard driver with scancode translation

**System Services**
- Basic process manager with idle process
- System call interface (INT 0x80) with stubs for exec, fork, etc.
- Virtual File System (VFS) layer with initrd support
- Kernel printf with VGA and serial output
- Interactive shell with polling-based keyboard input
- Recycle bin support with `recycle`, `clean rbin`, and `delete` shell commands
- Recycle bin auto-cleanup for files older than 30 days when shell starts

**ELF Loading**
- ELF binary loader with proper 32-bit struct definitions
- Page-by-page physical memory allocation for loaded segments
- Support for PT_LOAD segments with proper error handling
- Diagnostic output with actual memory addresses and sizes

**Runtime Support**
- C runtime helpers: memcpy, memset, panic
- Kernel status reporting with uptime and process info
- Memory stabilization tests
- Idle loop with periodic status updates

---

### Prerequisites

Install required packages on Debian/Ubuntu/Kali:

```bash
sudo apt update
sudo apt install -y build-essential gcc-multilib libc6-dev-i386 nasm binutils \
                    grub-pc-bin xorriso mtools qemu-system-i386
```

---

### Build and Run

From the project root:

```bash
# Build kernel
make all || make galio.bin


# Create GRUB ISO
chmod +x ./iso.sh
./iso.sh

# Run in QEMU with serial output
chmod +x ./run.sh
./run.sh

# Or run with VGA window
qemu-system-i386 -cdrom galio.iso -m 128M
```

## OR MAKE ALL IN ONCE AND RUN

make clean && rm -rf galio.iso && make all || make galio.bin && ./iso.sh && ./run.sh

---

### Project Layout

**Top Level**
- `Makefile` — Build rules for compiling and linking
- `galio.bin` — Linked kernel image
- `galio.iso` — Bootable GRUB ISO
- `README.md` — This documentation
- `initrd.bin` — Initial RAM disk image
- `disk.img` — Disk image for persistent storage

**Directories**

- `kernel/` — Core kernel sources
  - `arch/x86/boot/` — Boot code (boot.S, linker.ld)
  - `arch/x86/cpu/` — CPU management (GDT, IDT, IRQ, ISR)
  - `drivers/` — Device drivers (ATA, keyboard, serial, timer, VGA)
  - `fs/` — Filesystem drivers (EXT2, VFS)
  - `lib/` — Kernel libraries (kprintf, string, mem_test)
  - `mm/` — Memory management (paging, PMM, heap)
  - `process/` — Process management and ELF loader
  - `syscall/` — System call interface
  - `auth.c` — User authentication
  - `kmain.c` — Kernel entry point

- `include/` — Public headers
  - `arch/x86/` — Architecture headers (cpu, gdt, idt, irq)
  - `drivers/` — Driver headers (ata, keyboard, pit, serial, vga)
  - `fs/` — Filesystem headers (ext2, vfs, vfs_core)
  - `lib/` — Library headers (kprintf, string)
  - `mm/` — Memory management headers (heap, memory, paging, pmem)
  - `process/` — Process headers (elf, preempt, process, scheduler)
  - `syscall/` — Syscall header
  - `auth.h`, `common.h`, `init.h`

- `init/` — Init process
  - `init.c`

- `tools/` — Build tools
  - `mkiofs/` — Initrd generator
  - `shell/` — User shell with commands and editor

- `scripts/` — Utility scripts
  - `run.sh` — QEMU runner
  - `run_fullscreen.sh` — QEMU fullscreen runner
  - `iso.sh` — ISO creation script

- `test/` — Test programs
  - `test_elf.c` — ELF test binary

### Notes

- The kernel outputs boot progress to both VGA and serial (COM1)
- Serial output is recommended for debugging as it's more reliable
- The kernel enters an idle loop after initialization, printing status every second
- All major subsystems are initialized and functional
- Ready for extension with filesystem drivers, network stack, and userspace

---

### Development Status

✅ **Complete**: Boot, memory management, interrupts, drivers, processes, ELF loading, shell  
🔄 **Current**: Fixing paging and heap initialization for ELF segment loading  
🔄 **Next Steps**: Complete userspace support, filesystem implementation, scheduler enhancements

### Recent Updates (May 9, 2026)

- **ELF Loader Overhaul**: Fixed ELF header struct alignment and field types; implemented proper page-by-page physical memory allocation for loaded segments
- **Interactive Shell**: Replaced IRQ-driven keyboard with polling-based input for stable shell operation
- **Memory Management**: Enhanced error handling and diagnostic output in PMM and paging subsystems
- **Struct Definitions**: Corrected alignment issues in `elf_header_t` and `elf_program_header_t` with proper field sizes and removed unnecessary `__attribute__((packed))`
- **Constants**: Added PT_* segment type constants and PAGE_* flag definitions for better code clarity
