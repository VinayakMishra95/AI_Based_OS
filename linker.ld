ENTRY(_start)
OUTPUT_FORMAT(elf32-i386)

SECTIONS {
    . = 0x10000;

    .text ALIGN(4096) : {
        *(.text.entry)
        *(.text*)
    }

    .rodata ALIGN(4096) : {
        *(.rodata*)
    }

    .data ALIGN(4096) : {
        *(.data*)
    }

    .bss ALIGN(4096) : {
        __bss_start = .;
        *(COMMON)
        *(.bss*)
        __bss_end = .;
    }

    __kernel_end = .;
}
