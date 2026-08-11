; ============================================================
; bootloader.asm - Loads and executes C kernel
; Compile: nasm -f bin boot.asm -o bootloader.bin
; ============================================================

[org 0x7c00]
[bits 16]

start:
    ; Initialize segment registers
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00
    
    ; Print loading message
    mov si, msg_loading
    call print_string
    
    ; Reset disk system
    mov ah, 0x00
    mov dl, 0x80
    int 0x13
    
    ; Load kernel from disk (sectors 2-33 = 32 sectors = 16KB)
    mov ah, 0x02        ; BIOS read sectors function
    mov al, 32          ; Number of sectors to read
    mov ch, 0           ; Cylinder 0
    mov cl, 2           ; Start from sector 2 (1-indexed)
    mov dh, 0           ; Head 0
    mov dl, 0x80        ; First hard disk
    mov bx, 0x1000      ; Load kernel at 0x1000:0x0000
    int 0x13
    jc disk_error       ; Jump if carry flag set (error)
    
    ; Print OK message
    mov si, msg_ok
    call print_string
    
    ; Switch to 32-bit protected mode
    call switch_to_32bit

print_string:
    lodsb
    or al, al
    jz .done
    mov ah, 0x0e
    int 0x10
    jmp print_string
.done:
    ret

disk_error:
    mov si, msg_error
    call print_string
    jmp $

; ============================================================
; Switch to 32-bit Protected Mode
; ============================================================

switch_to_32bit:
    cli
    
    ; Load GDT
    lgdt [gdt_descriptor]
    
    ; Enable protected mode
    mov eax, cr0
    or eax, 0x1
    mov cr0, eax
    
    ; Far jump to 32-bit code
    jmp CODE_SEG:init_32bit

[bits 32]
init_32bit:
    ; Update segment registers
    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    ; Set up stack
    mov ebp, 0x90000
    mov esp, ebp
    
    ; Clear screen
    call clear_screen
    
    ; Print kernel start message
    push msg_kernel_start
    call print_32
    add esp, 4
    
    ; Call C kernel entry point
    call 0x1000
    
    ; If kernel returns, hang
    jmp $

; ============================================================
; 32-bit Functions
; ============================================================

clear_screen:
    pusha
    mov edi, 0xB8000
    mov eax, 0x0F20      ; Space with white on black
    mov ecx, 80*25
    rep stosw
    popa
    ret

print_32:
    push ebp
    mov ebp, esp
    pusha
    
    mov edi, 0xB8000
    mov ah, 0x0F         ; White on black
    mov esi, [ebp+8]     ; Get string pointer
    
.loop:
    lodsb
    test al, al
    jz .done
    stosw
    jmp .loop
    
.done:
    popa
    mov esp, ebp
    pop ebp
    ret

; ============================================================
; GDT
; ============================================================

gdt_start:
    ; Null descriptor
    dd 0x00000000
    dd 0x00000000
    
    ; Code segment (base=0, limit=4GB, 32-bit)
    gdt_code:
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 0b10011010
    db 0b11001111
    db 0x00
    
    ; Data segment (base=0, limit=4GB, 32-bit)
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

; ============================================================
; Data
; ============================================================

msg_loading:        db 'Loading kernel...', 0
msg_ok:             db 'OK', 13, 10, 0
msg_error:          db 'Disk error!', 0
msg_kernel_start:   db 'Kernel starting...', 13, 10, 0

; ============================================================
; Boot signature
; ============================================================

times 510 - ($ - $$) db 0
dw 0xAA55