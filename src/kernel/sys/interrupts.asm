; CardboardOS - Interrupt Handler Assembly Stubs
; These are the low-level interrupt handlers

BITS 64

section .text
global load_idt
global isr_wrapper
global irq_wrapper
global isr_stub_table
global irq_stub_table

; Load IDT
load_idt:
    mov rax, rdi
    lidt [rax]
    ret

; Interrupt Service Routine wrapper
isr_wrapper:
    ; Save registers
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    push rbp
    push rsp
    
    ; Call C handler
    call isr_handler
    
    ; Restore registers
    pop rsp
    pop rbp
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    
    ; Return from interrupt
    iretq

; IRQ wrapper
irq_wrapper:
    ; Same as ISR wrapper but calls irq_handler
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    push rbp
    push rsp
    
    call irq_handler
    
    pop rsp
    pop rbp
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    
    iretq

; ISR stub table
section .data
isr_stub_table:
    %assign i 0
    %rep 256
        dq isr_wrapper
    %assign i i+1
    %endrep

irq_stub_table:
    %assign i 0
    %rep 16
        dq irq_wrapper
    %assign i i+1
    %endrep