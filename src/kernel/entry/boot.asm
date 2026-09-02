; CardboardOS Boot Entry Point
; Uses NASM with 64-bit mode

BITS 64

; External symbols
extern kmain
extern __bss_start
extern __bss_end

; Export entry point
global _start

; Include multiboot header
%include "multiboot.asm"

section .text
_start:
    ; Disable interrupts
    cli
    
    ; Set up stack (RSP for 64-bit)
    mov rsp, stack_top
    
    ; Clear BSS (zero-initialized data)
    mov rdi, __bss_start
    mov rcx, __bss_end
    sub rcx, rdi
    xor rax, rax
    rep stosb
    
    ; Multiboot info from GRUB
    ; EBX contains the multiboot info structure
    ; EAX contains the magic number
    mov rdi, rbx  ; First argument: multiboot info pointer
    mov rsi, rax  ; Second argument: magic number
    
    ; Call kernel main
    call kmain
    
    ; Halt if kernel returns
.hang:
    hlt
    jmp .hang

section .bss
align 16
stack_bottom:
    resb 16384  ; 16KB stack
stack_top: