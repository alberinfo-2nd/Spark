ENTRY(_start)
OUTPUT_FORMAT(elf64-x86-64)

KERNEL_LMA = 0x100000; /*1MiB*/
KERNEL_VMA = 0xFFFFFFFF80000000;

SECTIONS
{
    . = KERNEL_LMA;
    .ltext : { *(.ltext) } /* Lower text */

    . = ALIGN(4k);
    .lrodata : { *(.lrodata) } /* Lower rodata */

    . = ALIGN(4k);
    .ldata : { *(.ldata) } /* Lower data */

    . = ALIGN(4k);
    .lbss : { *(.lbss) } /* Lower bss */

    KERNEL_LMA_END = ALIGN(4k);

    . += KERNEL_VMA;

    . = ALIGN(4k);    
    .text : AT(ADDR(.text) - KERNEL_VMA) { *(.text) }

    . = ALIGN(4k);
    .rodata : AT(ADDR(.rodata) - KERNEL_VMA) { *(.rodata) } 

    . = ALIGN(4k);
    .data : AT(ADDR(.data) - KERNEL_VMA) { *(.data) }

    . = ALIGN(4k);
    .bss : AT(ADDR(.bss) - KERNEL_VMA) { *(.bss) }
}