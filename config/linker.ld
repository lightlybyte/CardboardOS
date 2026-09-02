/* CardboardOS Linker Script */

ENTRY(_start)

SECTIONS
{
    /* Load at 1MB (Multiboot standard) */
    . = 0x100000;
    
    /* Multiboot header must be first */
    .multiboot : ALIGN(4K)
    {
        *(.multiboot)
    }
    
    /* Code section */
    .text : ALIGN(4K)
    {
        *(.text)
        *(.text.*)
    }
    
    /* Read-only data */
    .rodata : ALIGN(4K)
    {
        *(.rodata)
        *(.rodata.*)
    }
    
    /* Initialized data */
    .data : ALIGN(4K)
    {
        *(.data)
        *(.data.*)
    }
    
    /* Uninitialized data */
    .bss : ALIGN(4K)
    {
        __bss_start = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        __bss_end = .;
    }
    
    /* Kernel end */
    __kernel_end = .;
    
    /* Discard unwanted sections */
    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame)
        *(.note.*)
        *(.iplt)
        *(.igot)
        *(.rel.*)
    }
}