ENTRY(_start)

SECTIONS
{
    . = 0xFFFFFFFF80000000;

    .limine_requests : ALIGN(4096) {
        KEEP(*(.limine_requests_start))
        KEEP(*(.limine_requests))
        KEEP(*(.limine_requests_end))
    }

    .text : ALIGN(4096) {
        *(.text)
    }

    .rodata : ALIGN(4096) {
        *(.rodata)
    }

    .data : ALIGN(4096) {
        *(.data)
    }

    .bss : ALIGN(4096) {
        *(COMMON)
        *(.bss)
    }
}