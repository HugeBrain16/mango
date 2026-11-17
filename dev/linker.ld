ENTRY(start)
OUTPUT_FORMAT(binary)
OUTPUT_ARCH(i386)

SECTIONS {
    . = 0xA0000000;

    .program : {
        LONG(0x58474E4D)
        LONG(start - 0xA0000000)

        *(.text*)
        *(.rodata*)
        *(.data*)
        *(.bss*)
        *(COMMON)
    }
}
