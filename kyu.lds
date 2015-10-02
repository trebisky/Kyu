/* ld script to make skidoo
 * $Id: skidoo.lds,v 1.2 2002/09/11 05:09:44 tom Exp $
 * Modified from the linux .lds script, 8/2002 tjt
 * Modified from the skidoo .lds script, 9/2015 tjt
 *
 * Written by Martin Mares <mj@atrey.karlin.mff.cuni.cz>;
 */
/*
OUTPUT_FORMAT("elf32-i386", "elf32-i386", "elf32-i386")
*/
OUTPUT_ARCH(arm)
ENTRY(_begin)
SECTIONS
{
  /* Text and read-only data */
  . = 0x80300000;
  /*
  _text = .;			
  kyu_startup = .;
  */
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

  . = ALIGN(4096);		/* Init code and data */
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

  /* Stabs debugging sections.
  .stab 0 : { *(.stab) }
  .stabstr 0 : { *(.stabstr) }
  .stab.excl 0 : { *(.stab.excl) }
  .stab.exclstr 0 : { *(.stab.exclstr) }
  .stab.index 0 : { *(.stab.index) }
  .stab.indexstr 0 : { *(.stab.indexstr) }
  .comment 0 : { *(.comment) }
  */
}
