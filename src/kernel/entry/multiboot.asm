; CardboardOS Multiboot2 Header
; Must be in the first 8KB of the kernel

section .multiboot
align 8

; Multiboot2 magic number
MAGIC   equ 0xE85250D6
ARCH    equ 0              ; 0 = i386, 4 = MIPS
LENGTH  equ multiboot_header_end - multiboot_header
CHECKSUM equ -(MAGIC + ARCH + LENGTH)

multiboot_header:
    dd MAGIC
    dd ARCH
    dd LENGTH
    dd CHECKSUM

    ; End tag (required)
    align 8
    dd 0  ; Type
    dd 0  ; Flags
    dd 8  ; Size
multiboot_header_end: