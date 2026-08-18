; boot.asm - Multiboot compliant bootloader that loads kmain.c
; Build: nasm -f elf32 boot.asm -o boot.o
; Link: ld -m elf_i386 -Ttext 0x100000 -e kmain boot.o -o kernel.bin
; Create ISO: grub-mkrescue -o boot.iso iso/

[BITS 32]
[GLOBAL start]
[EXTERN kmain]      ; External C function

; Multiboot header - required for GRUB
section .multiboot
align 4
    dd 0x1BADB002          ; Magic number
    dd 0x03                ; Flags (0x03 = align on 4KB, provide memory info)
    dd -(0x1BADB002 + 0x03) ; Checksum

; Kernel code section
section .text
start:
    ; Set up stack pointer (stack grows down)
    mov esp, stack_end
    
    ; Clear EFLAGS
    push 0
    popf
    
    ; Save multiboot info pointer (in EBX) for C code
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