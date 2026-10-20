/* ovl.ld
 * linker script for kz-lite overlays (see include/ovl.h and tools/mkovl.py).
 * overlays are linked at OVL_BASE and relocated when loaded, so this must
 * be linked with --emit-relocs.
 */

OUTPUT_FORMAT("elf32-bigmips", "elf32-bigmips", "elf32-littlemips")
OUTPUT_ARCH(mips)
ENTRY(ovl_api)
SECTIONS
{
  . = 0x81000000 ;

  .text :
  {
    *(.text .text.*)
  }
  .rodata :
  {
    *(.rdata .rdata.*)
    *(.rodata .rodata.*)
  }
  .data :
  {
    KEEP(*(.ovl_api))
    *(.data .data.*)
    *(.sdata .sdata.*)
    *(.lit8)
    *(.lit4)
  }
  .bss (NOLOAD) :
  {
    *(.sbss .sbss.*)
    *(.scommon)
    *(.bss .bss.*)
    *(COMMON)
  }

  /DISCARD/ :
  {
    *(.MIPS.abiflags)
    *(.reginfo)
    *(.pdr)
    *(.comment)
    *(.gnu.attributes)
    *(.mdebug.*)
    *(.eh_frame .eh_frame.*)
    *(.ctors .dtors .init .fini)
  }
}
