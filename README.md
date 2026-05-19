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

**Directories**

- `boot/` — Boot code and linker script
  - `boot.S` — Multiboot header and entry point
  - `linker.ld` — Linker script for ELF layout

- `include/` — Public headers
  - `common.h` — Common types and utilities
  - `cpu.h` — CPU structures and I/O functions
  - `vga.h` — VGA driver interface
  - `serial.h` — Serial driver interface
  - `gdt.h` — GDT management
  - `idt.h` — IDT management
  - `irq.h` — Interrupt handling
  - `pmem.h` — Physical memory manager
  - `paging.h` — Virtual memory paging
  - `heap.h` — Heap allocator
  - `pit.h` — Timer driver
  - `keyboard.h` — Keyboard driver
  - `process.h` — Process management
  - `syscall.h` — System call interface
  - `vfs.h` — Virtual filesystem
  - `kprintf.h` — Kernel printf

- `src/` — Kernel source files
  - `asm.s` — Assembly utilities (GDT/IDT loading)
  - `isr_asm.s` — ISR/IRQ assembly stubs
  - `kernel.c` — Core utilities and panic
  - `kmain.c` — Main kernel entry and initialization
  - `vga.c` — VGA text driver
  - `serial.c` — Serial port driver
  - `gdt.c` — GDT setup
  - `idt.c` — IDT setup
  - `irq.c` — PIC and interrupt management
  - `isr.c` — ISR/IRQ handlers
  - `pmem.c` — Physical memory allocation
  - `paging.c` — Virtual memory paging
  - `heap.c` — Kernel heap
  - `pit.c` — Timer driver
  - `keyboard.c` — Keyboard input
  - `process.c` — Process management
  - `syscall.c` — System calls
  - `vfs.c` — Filesystem layer
  - `string.c` — String utilities
  - `elf.c` — ELF loading utilities

---

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
