[org 0x7c00]
[bits 16]

start:
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00
    
    mov si, msg
    call print
    
    ; Load kernel at 0x1000:0x0000
    mov ax, 0x1000
    mov es, ax
    xor bx, bx
    
    mov ah, 0x02
    mov al, 20
    mov ch, 0
    mov cl, 2
    mov dh, 0
    mov dl, 0x00
    int 0x13
    jc disk_error
    
    mov si, msg_ok
    call print
    
    ; Switch to protected mode
    cli
    lgdt [gdt_descriptor]
    
    mov eax, cr0
    or eax, 0x1
    mov cr0, eax
    
    jmp CODE_SEG:init_32bit

[bits 32]
init_32bit:
    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    mov ebp, 0x90000
    mov esp, ebp
    
    ; Write debug message
    mov edi, 0xB8000
    add edi, 160
    mov esi, msg_protected
    call print_string_32
    
    ; Jump to kernel using absolute address
    ; Since we loaded at 0x1000:0x0000, physical address is 0x10000
    mov eax, 0x10000
    call eax
    
    ; If kernel returns, halt
    jmp $

print_string_32:
    pusha
    mov ah, 0x0F
.loop:
    lodsb
    test al, al
    jz .done
    stosw
    jmp .loop
.done:
    popa
    ret

print:
    lodsb
    or al, al
    jz .done
    mov ah, 0x0e
    int 0x10
    jmp print
.done:
    ret

disk_error:
    mov si, msg_error
    call print
    jmp $

; GDT
gdt_start:
    dd 0x00000000
    dd 0x00000000
    gdt_code:
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 0b10011010
    db 0b11001111
    db 0x00
    gdt_data:
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 0b10010010
    db 0b11001111
    db 0x00
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

CODE_SEG equ gdt_code - gdt_start
DATA_SEG equ gdt_data - gdt_start

msg:                db 'Loading kernel...', 13, 10, 0
msg_ok:             db 'OK', 13, 10, 0
msg_error:          db 'Error!', 13, 10, 0
msg_protected:      db '32-bit mode!', 0

times 510 - ($ - $$) db 0
dw 0xAA55