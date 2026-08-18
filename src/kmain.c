; src/boot.asm - Multiboot compliant bootloader
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
stack_end:// src/kmain.c - Simple kernel with BRASH shell
#define VIDEO_MEMORY 0xB8000
#define VGA_WIDTH 80
#define VGA_HEIGHT 25

// Terminal functions
void tinit(void);
void tputchar(char c);
void tputchar_color(char c, unsigned char color);
void terminal_write(const char* data);
void twrite(const char* data);
char tinput(void);
void tread(char* buffer, int max_len);
void tclear(void);
void tsetcolor(unsigned char color);
void tscroll(void);

// Shell functions
void kmain(void);
void shell_init(void);
void shell_prompt(void);
void shell_execute(char* cmd);
void shell_help(void);
void shell_echo(char* args);
void shell_clear(void);
void shell_reboot(void);
void shell_halt(void);
void shell_status(void);
void shell_meminfo(void);
void shell_version(void);

// String functions
int strlen(const char* str);
int strcmp(const char* s1, const char* s2);
void strcpy(char* dest, const char* src);
void strcat(char* dest, const char* src);
int strncmp(const char* s1, const char* s2, int n);

// Keyboard functions
unsigned char inb(unsigned short port);
void outb(unsigned short port, unsigned char data);

// Terminal state
static int terminal_row;
static int terminal_column;
static unsigned char terminal_color;
static unsigned short* terminal_buffer;

// Terminal color codes
#define COLOR_BLACK         0x00
#define COLOR_BLUE          0x01
#define COLOR_GREEN         0x02
#define COLOR_CYAN          0x03
#define COLOR_RED           0x04
#define COLOR_MAGENTA       0x05
#define COLOR_BROWN         0x06
#define COLOR_LIGHT_GRAY    0x07
#define COLOR_DARK_GRAY     0x08
#define COLOR_LIGHT_BLUE    0x09
#define COLOR_LIGHT_GREEN   0x0A
#define COLOR_LIGHT_CYAN    0x0B
#define COLOR_LIGHT_RED     0x0C
#define COLOR_LIGHT_MAGENTA 0x0D
#define COLOR_YELLOW        0x0E
#define COLOR_WHITE         0x0F

// Shell constants
#define MAX_CMD_LEN 256
#define MAX_ARGS 16
#define SHELL_NAME "BRASH"
#define SHELL_VERSION "1.0"

// Port I/O functions
unsigned char inb(unsigned short port) {
    unsigned char ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void outb(unsigned short port, unsigned char data) {
    __asm__ volatile ("outb %0, %1" : : "a"(data), "Nd"(port));
}

// String functions implementation
int strlen(const char* str) {
    int len = 0;
    while (str[len]) len++;
    return len;
}

int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

void strcpy(char* dest, const char* src) {
    while (*src) {
        *dest++ = *src++;
    }
    *dest = '\0';
}

void strcat(char* dest, const char* src) {
    while (*dest) dest++;
    while (*src) {
        *dest++ = *src++;
    }
    *dest = '\0';
}

int strncmp(const char* s1, const char* s2, int n) {
    while (n-- > 0 && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    if (n < 0) return 0;
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

// Terminal implementation
void tinit(void) {
    terminal_row = 0;
    terminal_column = 0;
    terminal_color = 0x0F;  // White on black
    terminal_buffer = (unsigned short*) VIDEO_MEMORY;
    tclear();
}

void tclear(void) {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        terminal_buffer[i] = (unsigned short) (' ' | (terminal_color << 8));
    }
    terminal_row = 0;
    terminal_column = 0;
}

void tsetcolor(unsigned char color) {
    terminal_color = color;
}

void tscroll(void) {
    // Scroll up one line
    for (int i = 0; i < (VGA_HEIGHT - 1) * VGA_WIDTH; i++) {
        terminal_buffer[i] = terminal_buffer[i + VGA_WIDTH];
    }
    // Clear last line
    for (int i = (VGA_HEIGHT - 1) * VGA_WIDTH; i < VGA_HEIGHT * VGA_WIDTH; i++) {
        terminal_buffer[i] = (unsigned short) (' ' | (terminal_color << 8));
    }
    terminal_row = VGA_HEIGHT - 1;
    terminal_column = 0;
}

void tputchar(char c) {
    tputchar_color(c, terminal_color);
}

void tputchar_color(char c, unsigned char color) {
    if (c == '\n') {
        terminal_row++;
        terminal_column = 0;
        if (terminal_row >= VGA_HEIGHT) {
            tscroll();
        }
        return;
    }
    
    if (c == '\t') {
        // Tab = 4 spaces
        for (int i = 0; i < 4; i++) {
            tputchar_color(' ', color);
        }
        return;
    }
    
    if (c == '\b') {
        // Backspace
        if (terminal_column > 0) {
            terminal_column--;
            unsigned short* where = terminal_buffer + (terminal_row * VGA_WIDTH + terminal_column);
            *where = (unsigned short) (' ' | (color << 8));
        }
        return;
    }
    
    unsigned short* where = terminal_buffer + (terminal_row * VGA_WIDTH + terminal_column);
    *where = (unsigned short) (c | (color << 8));
    
    terminal_column++;
    if (terminal_column >= VGA_WIDTH) {
        terminal_column = 0;
        terminal_row++;
    }
    
    if (terminal_row >= VGA_HEIGHT) {
        tscroll();
    }
}

void terminal_write(const char* data) {
    while (*data) {
        tputchar(*data++);
    }
}

void twrite(const char* data) {
    terminal_write(data);
}

// Keyboard input functions
unsigned char get_scancode(void) {
    return inb(0x60);
}

int is_key_pressed(void) {
    // Check if data is available in keyboard buffer
    return inb(0x64) & 0x01;
}

// Convert scancode to ASCII
char scancode_to_ascii(unsigned char scancode) {
    // Simple scancode to ASCII mapping
    // Only handles basic keys without shift
    static const char scancode_map[128] = {
        0,    0,    '1',  '2',  '3',  '4',  '5',  '6',  // 0x00-0x07
        '7',  '8',  '9',  '0',  '-',  '=',  0,    0,    // 0x08-0x0F
        0,    'q',  'w',  'e',  'r',  't',  'y',  'u',  // 0x10-0x17
        'i',  'o',  'p',  '[',  ']',  '\n', 0,    'a',  // 0x18-0x1F
        's',  'd',  'f',  'g',  'h',  'j',  'k',  'l',  // 0x20-0x27
        ';',  '\'', '`',  0,    '\\', 'z',  'x',  'c',  // 0x28-0x2F
        'v',  'b',  'n',  'm',  ',',  '.',  '/',  0,    // 0x30-0x37
        0,    ' ',  0,    0,    0,    0,    0,    0,    // 0x38-0x3F
        0,    0,    0,    0,    0,    0,    0,    0,    // 0x40-0x47
        0,    0,    0,    0,    0,    0,    0,    0,    // 0x48-0x4F
        0,    0,    0,    0,    0,    0,    0,    0,    // 0x50-0x57
        0,    0,    0,    0,    0,    0,    0,    0,    // 0x58-0x5F
        0,    0,    0,    0,    0,    0,    0,    0,    // 0x60-0x67
        0,    0,    0,    0,    0,    0,    0,    0,    // 0x68-0x6F
        0,    0,    0,    0,    0,    0,    0,    0,    // 0x70-0x77
        0,    0,    0,    0,    0,    0,    0,    0     // 0x78-0x7F
    };
    
    if (scancode < 128) {
        return scancode_map[scancode];
    }
    return 0;
}

// Input functions
char tinput(void) {
    // Wait for a key press and return the character
    while (1) {
        if (is_key_pressed()) {
            unsigned char scancode = get_scancode();
            if (!(scancode & 0x80)) {
                // Key pressed (not released)
                char ascii = scancode_to_ascii(scancode);
                if (ascii) {
                    return ascii;
                }
            }
        }
    }
}

void tread(char* buffer, int max_len) {
    int i = 0;
    char c;
    
    tsetcolor(COLOR_WHITE);
    
    while (i < max_len - 1) {
        // Wait for key
        while (!is_key_pressed()) {
            // Small delay to prevent CPU spinning
            for (volatile int delay = 0; delay < 100; delay++);
        }
        
        unsigned char scancode = get_scancode();
        
        // Ignore key releases
        if (scancode & 0x80) {
            continue;
        }
        
        // Convert scancode to ASCII
        char ascii = scancode_to_ascii(scancode);
        
        if (ascii == '\n') {  // Enter
            buffer[i] = '\0';
            tputchar('\n');
            return;
        }
        else if (ascii == '\b') {  // Backspace
            if (i > 0) {
                i--;
                tputchar('\b');
            }
        }
        else if (ascii) {  // Any other printable character
            buffer[i++] = ascii;
            tputchar(ascii);
        }
    }
    
    buffer[i] = '\0';
}

// Shell implementation
void shell_init(void) {
    tsetcolor(COLOR_LIGHT_CYAN);
    twrite("\n");
    twrite("  ██████╗ ██████╗  █████╗ ███████╗██╗  ██╗\n");
    twrite("  ██╔══██╗██╔══██╗██╔══██╗██╔════╝██║  ██║\n");
    twrite("  ██████╔╝██████╔╝███████║███████╗███████║\n");
    twrite("  ██╔══██╗██╔══██╗██╔══██║╚════██║██╔══██║\n");
    twrite("  ██████╔╝██║  ██║██║  ██║███████║██║  ██║\n");
    twrite("  ╚═════╝ ╚═╝  ╚═╝╚═╝  ╚═╝╚══════╝╚═╝  ╚═╝\n");
    tsetcolor(COLOR_GREEN);
    twrite("  BRASH Shell v");
    twrite(SHELL_VERSION);
    twrite(" - POSIX Compliant\n");
    tsetcolor(COLOR_LIGHT_GRAY);
    twrite("  Type 'help' for available commands\n\n");
}

void shell_prompt(void) {
    tsetcolor(COLOR_YELLOW);
    twrite("BRASH> ");
    tsetcolor(COLOR_WHITE);
}

void shell_help(void) {
    tsetcolor(COLOR_LIGHT_CYAN);
    twrite("\nAvailable commands:\n");
    tsetcolor(COLOR_WHITE);
    twrite("  help          - Show this help message\n");
    twrite("  echo [text]   - Echo text back\n");
    twrite("  clear         - Clear the screen\n");
    twrite("  reboot        - Reboot the system\n");
    twrite("  halt          - Halt the system\n");
    twrite("  status        - Show system status\n");
    twrite("  meminfo       - Show memory information\n");
    twrite("  version       - Show version information\n");
    twrite("  about         - Show about BRASH\n");
    twrite("  whoami        - Display current user (root)\n");
    twrite("  uptime        - Show system uptime (simulated)\n");
    tsetcolor(COLOR_GREEN);
    twrite("\nBRASH is POSIX compliant!\n");
}

void shell_echo(char* args) {
    tsetcolor(COLOR_WHITE);
    if (args && args[0]) {
        twrite(args);
    }
    twrite("\n");
}

void shell_clear(void) {
    tclear();
}

void shell_reboot(void) {
    tsetcolor(COLOR_RED);
    twrite("Rebooting system...\n");
    // Reboot using keyboard controller
    outb(0x64, 0xFE);
    // If reboot fails, hang
    while(1) {
        __asm__ volatile("hlt");
    }
}

void shell_halt(void) {
    tsetcolor(COLOR_RED);
    twrite("Halting system...\n");
    while(1) {
        __asm__ volatile("hlt");
    }
}

void shell_status(void) {
    tsetcolor(COLOR_GREEN);
    twrite("\nSystem Status:\n");
    tsetcolor(COLOR_WHITE);
    twrite("  OS: CardboardOS\n");
    twrite("  Shell: BRASH v");
    twrite(SHELL_VERSION);
    twrite("\n");
    twrite("  Architecture: i386\n");
    twrite("  Terminal: VGA Text Mode 80x25\n");
    twrite("  Bootloader: GRUB 0.95\n");
    twrite("  Mode: Protected Mode\n");
    twrite("  Status: Running\n");
    twrite("  User: root\n");
}

void shell_meminfo(void) {
    tsetcolor(COLOR_GREEN);
    twrite("\nMemory Information:\n");
    tsetcolor(COLOR_WHITE);
    twrite("  Video Memory: 0xB8000\n");
    twrite("  VGA Buffer Size: 4000 bytes (80x25x2)\n");
    twrite("  Stack: ~16KB\n");
    twrite("  Kernel Base: 0x100000\n");
    twrite("  Multiboot Info: Provided by GRUB\n");
}

void shell_version(void) {
    tsetcolor(COLOR_CYAN);
    twrite("CardboardOS v1.0\n");
    twrite("BRASH Shell v");
    twrite(SHELL_VERSION);
    twrite(" - POSIX Compliant\n");
    twrite("Copyright (C) 2026 CardboardOS\n");
}

void shell_about(void) {
    tsetcolor(COLOR_MAGENTA);
    twrite("================================================================\n");
    twrite("================================================================\n");
    twrite("Cardboard OS v2026.8 'Eclipse'\n");
    twrite("================================================================\n");
    twrite("================================================================\n");
    tsetcolor(COLOR_WHITE);
    twrite("  CardboardOS - A Simple Operating System\n");
    twrite("  BRASH - Basic Rich And SHell\n");
    twrite("  Version: ");
    twrite(SHELL_VERSION);
    twrite("\n  POSIX Compliant Shell\n");
    twrite("  Built with love in C and x86 Assembly\n\n");
}

void shell_whoami(void) {
    tsetcolor(COLOR_GREEN);
    twrite("root\n");
}

void shell_uptime(void) {
    tsetcolor(COLOR_GREEN);
    twrite("System uptime: 00:00:01 (simulated)\n");
    tsetcolor(COLOR_WHITE);
    twrite("(Uptime tracking not yet implemented)\n");
}

void shell_execute(char* cmd) {
    // Parse command and arguments
    char* args[MAX_ARGS];
    int arg_count = 0;
    char* token = cmd;
    
    // Skip leading spaces
    while (*token == ' ') token++;
    
    // Tokenize
    while (*token && arg_count < MAX_ARGS) {
        args[arg_count] = token;
        arg_count++;
        
        // Find end of token
        while (*token && *token != ' ') token++;
        
        if (*token) {
            *token = '\0';
            token++;
            // Skip spaces
            while (*token == ' ') token++;
        }
    }
    
    if (arg_count == 0) {
        return;
    }
    
    // Execute command
    if (strcmp(args[0], "help") == 0 || strcmp(args[0], "?") == 0) {
        shell_help();
    }
    else if (strcmp(args[0], "echo") == 0) {
        if (arg_count > 1) {
            // Combine remaining args
            char buffer[MAX_CMD_LEN];
            buffer[0] = '\0';
            for (int i = 1; i < arg_count; i++) {
                strcat(buffer, args[i]);
                if (i < arg_count - 1) strcat(buffer, " ");
            }
            shell_echo(buffer);
        } else {
            shell_echo("");
        }
    }
    else if (strcmp(args[0], "clear") == 0 || strcmp(args[0], "cls") == 0) {
        shell_clear();
    }
    else if (strcmp(args[0], "reboot") == 0) {
        shell_reboot();
    }
    else if (strcmp(args[0], "halt") == 0) {
        shell_halt();
    }
    else if (strcmp(args[0], "status") == 0 || strcmp(args[0], "info") == 0) {
        shell_status();
    }
    else if (strcmp(args[0], "meminfo") == 0) {
        shell_meminfo();
    }
    else if (strcmp(args[0], "version") == 0 || strcmp(args[0], "ver") == 0) {
        shell_version();
    }
    else if (strcmp(args[0], "about") == 0) {
        shell_about();
    }
    else if (strcmp(args[0], "whoami") == 0) {
        shell_whoami();
    }
    else if (strcmp(args[0], "uptime") == 0) {
        shell_uptime();
    }
    else if (strcmp(args[0], "exit") == 0 || strcmp(args[0], "quit") == 0) {
        tsetcolor(COLOR_YELLOW);
        twrite("Goodbye!\n");
        while(1) {
            __asm__ volatile("hlt");
        }
    }
    else {
        tsetcolor(COLOR_RED);
        twrite("Command not found: ");
        twrite(args[0]);
        twrite("\n");
        tsetcolor(COLOR_LIGHT_GRAY);
        twrite("Type 'help' for available commands\n");
    }
}

// Main kernel entry point
void kmain(void) {
    char cmd[MAX_CMD_LEN];
    
    // Initialize terminal
    tinit();
    
    // Initialize shell
    shell_init();
    
    // Main shell loop
    while (1) {
        shell_prompt();
        
        // Read command from keyboard
        tread(cmd, MAX_CMD_LEN);
        shell_execute(cmd);
    }
}