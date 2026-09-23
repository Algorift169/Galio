/* boot/linker.ld - Place the Multiboot2 header in the first PT_LOAD.
 * GRUB searches the first 32 KiB for the header. */

OUTPUT_FORMAT("elf64-x86-64")
ENTRY(start)

SECTIONS
{
  . = 0x00100000;

  .text ALIGN(4K) : {
    KEEP(*(.multiboot2))
    KEEP(*(.ap_trampoline))
    *(.text)
    *(.text.*)
    *(.rodata)
    *(.rodata.*)
  }

  .data ALIGN(4K) : {
    *(.data)
    *(.data.*)
  }

  .bss ALIGN(4K) : {
    *(.bss)
    *(COMMON)
  }

  /DISCARD/ : {
    *(.eh_frame)
    *(.comment)
  }
}