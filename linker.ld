OUTPUT_FORMAT(binary)

ENTRY(init)

SECTIONS {
    . = 0xc0000000;

    .text.init : {
        src/kernel.o(.text)
    }

    .text : {
        *(.text)
    }

    .data : {
        *(.data)
    }

    textend = ALIGN(0x1000);

    .bss ALIGN(0x1000) : {
        *(.bss)
    }

    end = ALIGN(0x1000);
}
