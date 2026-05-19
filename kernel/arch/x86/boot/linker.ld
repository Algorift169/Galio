/* boot/linker.ld - Linker script placing the multiboot header into the first PT_LOAD
 *
 * This script ensures the .multiboot section is emitted at the very start of
 * the first loadable segment so GRUB can find the Multiboot v1 magic within
 * the first 8 KiB of the file.
 */

OUTPUT_FORMAT("elf32-i386")
ENTRY(start)

SECTIONS
{
  /* Put the multiboot header at the start of the first loadable segment.
     KEEP prevents the linker from discarding the section. */
  .text ALIGN(4) : {
    KEEP(*(.multiboot))
    *(.text)
    *(.text.*)
    *(.rodata)
  }

  /* Set the virtual load address for the kernel in memory */
  . = 0x00100000;

  .data ALIGN(4) : {
    *(.data)
    *(.data.*)
  }

  .bss ALIGN(4) : {
    *(.bss)
    *(COMMON)
  }

  /* Discard unwelcome sections */
  /DISCARD/ : { *(.eh_frame) }
}