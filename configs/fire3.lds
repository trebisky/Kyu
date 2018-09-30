/* ld script to make kyu for the ARMv8 fire3 samsung s5p6818
 *
 * Modified from the linux .lds script, 8/2002 tjt
 * Modified from the skidoo .lds script, 9/2015 tjt
 * Modified from the BBB .lds script, 1/2017 tjt
 * Modified from the OrangePI .lds script, 9/2018 tjt
 *
 * Written by Martin Mares <mj@atrey.karlin.mff.cuni.cz>;
 */

OUTPUT_FORMAT("elf64-littleaarch64", "elf64-littleaarch64", "elf64-littleaarch64")
OUTPUT_ARCH(aarch64)

ENTRY(_begin)
SECTIONS
{
  /* Text and read-only data */
  . = 0x40000000;
  .text : {
	*(.text)
	*(.text.unlikely)
	}

  _etext = .;			/* End of text section */

  .rodata : { *(.rodata) *(.rodata.*) }

  .data : {			/* Data */
	*(.data)
	*(.data.unlikely)
	}

  _edata = .;			/* End of data section */

  /* Init code and data */
  /* This business is Linux specific,
   *   and we don't need it.
   */
  /*
  . = ALIGN(4096);
  __init_begin = .;
  .text.init : { *(.text.init) }
  .data.init : { *(.data.init) }

  . = ALIGN(16);
  __setup_start = .;
  .setup.init : { *(.setup.init) }
  __setup_end = .;

  __initcall_start = .;
  .initcall.init : { *(.initcall.init) }
  __initcall_end = .;
  . = ALIGN(4096);

  __init_end = .;
  */

  __bss_start__ = .;		/* BSS */
  .bss : {
	*(.bss)
	}
  __bss_end__ = . ;
  _end = . ;

  /* Sections to be discarded */
  /DISCARD/ : {
	*(.text.exit)
	*(.data.exit)
	*(.exitcall.exit)
	}
}
