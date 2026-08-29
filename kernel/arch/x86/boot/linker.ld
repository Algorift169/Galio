/* boot/linker.ld - Linker script placing the multiboot header into the first PT_LOAD
 *
 * This script ensures the .multiboot section is emitted at the very start of
 * the first loadable segment so GRUB can find the Multiboot v1 magic within
 * the first 8 KiB of the file.
 */

OUTPUT_FORMAT("elf64-x86-64")
ENTRY(start)

SECTIONS
{
  . = 0x00100000;

  .text ALIGN(4K) : {
    KEEP(*(.multiboot))
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