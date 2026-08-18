; src/boot.asm - Multiboot compliant bootloader with keyboard support
; Build: nasm -f elf32 src/boot.asm -o build/boot.o

[BITS 32]
[GLOBAL start]
[GLOBAL idt_load]
[GLOBAL keyboard_handler]
[EXTERN kmain]      ; External C function
[EXTERN keyboard_handler_main]  ; C keyboard handler

; Multiboot header - GRUB 0.95 compatible
section .multiboot
align 4
    dd 0x1BADB002          ; Magic number
    dd 0x03                ; Flags: align on 4KB, provide memory info
    dd -(0x1BADB002 + 0x03) ; Checksum

; Global Descriptor Table
section .text
start:
    ; Set up stack pointer (16KB stack)
    mov esp, stack_end
    
    ; Clear EFLAGS
    push 0
    popf
    
    ; Save multiboot info pointer for C code
    push ebx
    
    ; Set up IDT for keyboard interrupts
    call setup_idt
    
    ; Enable interrupts
    sti
    
    ; Call kmain
    call kmain
    
    ; If kmain returns, hang
    cli
    hlt
    jmp $

; Keyboard interrupt handler (IRQ1)
keyboard_handler:
    pusha
    push ds
    push es
    push fs
    push gs
    
    ; Set up data segment
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    
    ; Call C handler
    call keyboard_handler_main
    
    ; Send EOI to PIC
    mov al, 0x20
    out 0x20, al
    
    pop gs
    pop fs
    pop es
    pop ds
    popa
    iret

; Set up IDT
setup_idt:
    ; Point IDT to keyboard_handler
    mov eax, keyboard_handler
    mov [idt], ax                    ; Low 16 bits of handler
    mov [idt+2], word 0x08           ; Code segment selector
    mov [idt+4], word 0x8E00         ; Present, ring 0, interrupt gate
    shr eax, 16
    mov [idt+6], ax                  ; High 16 bits of handler
    
    ; Load IDT
    lidt [idt_desc]
    ret

; Interrupt Descriptor Table
section .data
align 8
idt:
    ; Reserved for keyboard (IRQ1)
    times 8 db 0
    ; Keyboard IRQ1
    dw 0  ; Will be filled in
    dw 0x08
    db 0
    db 0x8E
    dw 0

idt_desc:
    dw (8 * 256) - 1  ; Size of IDT
    dd idt            ; Address of IDT

; Stack section - 16KB stack
section .bss
align 16
stack_bottom:
    resb 16384  ; 16KB stack
stack_end:

; Export symbols for C
global idt
global keyboard_handler