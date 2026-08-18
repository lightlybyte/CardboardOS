; src/boot.asm - Multiboot compliant bootloader for GRUB 0.95 / Paperboot 26.8
; Build: nasm -f elf32 src/boot.asm -o build/boot.o

[BITS 32]
[GLOBAL start]
[EXTERN kmain]      ; External C function

; Multiboot header - GRUB 0.95 compatible
section .multiboot
align 4
    dd 0x1BADB002          ; Magic number
    dd 0x03                ; Flags: align on 4KB, provide memory info
    dd -(0x1BADB002 + 0x03) ; Checksum

; Kernel code section
section .text
start:
    ; Set up stack pointer (16KB stack)
    mov esp, stack_end
    
    ; Clear EFLAGS
    push 0
    popf
    
    ; Save multiboot info pointer for C code
    push ebx
    
    ; Call kmain
    call kmain
    
    ; If kmain returns, hang
    cli
    hlt
    jmp $

; Stack section - 16KB stack
section .bss
align 16
stack_bottom:
    resb 16384  ; 16KB stack
stack_end: