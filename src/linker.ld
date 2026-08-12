OUTPUT_FORMAT(elf32-i386)
ENTRY(kmain)

SECTIONS {
    . = 0x10000;
    
    .text : {
        *(.text)
        *(.text.*)
    }
    
    .rodata : {
        *(.rodata)
        *(.rodata.*)
    }
    
    .data : {
        *(.data)
        *(.data.*)
    }
    
    .bss : {
        *(.bss)
        *(.bss.*)
        *(COMMON)
    }
    
    /* Discard unwanted sections */
    /DISCARD/ : {
        *(.eh_frame)
        *(.comment)
        *(.note.*)
    }
}