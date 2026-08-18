; src/boot.asm - Multiboot compliant bootloader (Optimized)
; Build: nasm -f elf32 src/boot.asm -o build/boot.o

[BITS 32]
[GLOBAL start]
[EXTERN kmain]

; Multiboot header
section .multiboot
align 4
    dd 0x1BADB002
    dd 0x03
    dd -(0x1BADB002 + 0x03)

; Kernel code section
section .text
start:
    ; Set up stack pointer (4KB stack - optimized)
    mov esp, stack_end
    
    ; Clear EFLAGS
    push 0
    popf
    
    ; Save multiboot info
    push ebx
    
    ; Call kmain
    call kmain
    
    ; Hang if returns
    cli
    hlt
    jmp $

; Stack section - 4KB stack (reduced from 16KB)
section .bss
align 16
stack_bottom:
    resb 4096  ; 4KB stack
stack_end: