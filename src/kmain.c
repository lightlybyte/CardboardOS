// src/kmain.c - Simple kernel with BRASH shell (Optimized)
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
void shell_neofetch(void);

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
#define MAX_CMD_LEN 128      // Reduced from 256
#define MAX_ARGS 8           // Reduced from 16
#define SHELL_NAME "BRASH"
#define SHELL_VERSION "1.0"

// Stack size (reduced from 16KB to 4KB)
#define STACK_SIZE 4096

// Port I/O functions
unsigned char inb(unsigned short port) {
    unsigned char ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void outb(unsigned short port, unsigned char data) {
    __asm__ volatile ("outb %0, %1" : : "a"(data), "Nd"(port));
}

// String functions - Optimized
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

// Terminal implementation - Optimized
void tinit(void) {
    terminal_row = 0;
    terminal_column = 0;
    terminal_color = 0x0F;
    terminal_buffer = (unsigned short*) VIDEO_MEMORY;
    tclear();
}

void tclear(void) {
    unsigned short blank = (unsigned short) (' ' | (terminal_color << 8));
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        terminal_buffer[i] = blank;
    }
    terminal_row = 0;
    terminal_column = 0;
}

void tsetcolor(unsigned char color) {
    terminal_color = color;
}

void tscroll(void) {
    // Scroll up one line efficiently
    for (int i = 0; i < (VGA_HEIGHT - 1) * VGA_WIDTH; i++) {
        terminal_buffer[i] = terminal_buffer[i + VGA_WIDTH];
    }
    unsigned short blank = (unsigned short) (' ' | (terminal_color << 8));
    for (int i = (VGA_HEIGHT - 1) * VGA_WIDTH; i < VGA_HEIGHT * VGA_WIDTH; i++) {
        terminal_buffer[i] = blank;
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
        for (int i = 0; i < 4; i++) {
            tputchar_color(' ', color);
        }
        return;
    }
    
    if (c == '\b') {
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
    return inb(0x64) & 0x01;
}

// Scancode to ASCII mapping
char scancode_to_ascii(unsigned char scancode) {
    static const char scancode_map[128] = {
        0,    0,    '1',  '2',  '3',  '4',  '5',  '6',
        '7',  '8',  '9',  '0',  '-',  '=',  '\b', '\t',
        'q',  'w',  'e',  'r',  't',  'y',  'u',  'i',
        'o',  'p',  '[',  ']',  '\n', 0,    'a',  's',
        'd',  'f',  'g',  'h',  'j',  'k',  'l',  ';',
        '\'', '`',  0,    '\\', 'z',  'x',  'c',  'v',
        'b',  'n',  'm',  ',',  '.',  '/',  0,    '*',
        0,    ' ',  0,    0,    0,    0,    0,    0,
        0,    0,    0,    0,    0,    0,    0,    0,
        0,    0,    0,    0,    0,    0,    0,    0,
        0,    0,    0,    0,    0,    0,    0,    0,
        0,    0,    0,    0,    0,    0,    0,    0,
        0,    0,    0,    0,    0,    0,    0,    0,
        0,    0,    0,    0,    0,    0,    0,    0,
        0,    0,    0,    0,    0,    0,    0,    0,
        0,    0,    0,    0,    0,    0,    0,    0
    };
    
    if (scancode < 128) {
        return scancode_map[scancode];
    }
    return 0;
}

void tread(char* buffer, int max_len) {
    int i = 0;
    
    tsetcolor(COLOR_WHITE);
    
    while (i < max_len - 1) {
        while (!is_key_pressed()) {
            for (volatile int delay = 0; delay < 50; delay++);
        }
        
        unsigned char scancode = get_scancode();
        
        if (scancode & 0x80) {
            continue;
        }
        
        char ascii = scancode_to_ascii(scancode);
        
        if (ascii == '\n') {
            buffer[i] = '\0';
            tputchar('\n');
            return;
        }
        else if (ascii == '\b') {
            if (i > 0) {
                i--;
                tputchar('\b');
                buffer[i] = '\0';
            }
        }
        else if (ascii == '\t') {
            for (int j = 0; j < 4 && i < max_len - 1; j++) {
                buffer[i++] = ' ';
                tputchar(' ');
            }
        }
        else if (ascii && ascii >= ' ' && ascii <= '~') {
            buffer[i++] = ascii;
            tputchar(ascii);
        }
    }
    
    buffer[i] = '\0';
}

// Neofetch - Optimized with minimal strings
void shell_neofetch(void) {
    // Store ASCII art as single strings to reduce overhead
    tsetcolor(COLOR_CYAN);
    twrite("\n");
    twrite("       .,:lodddddddddddddddddddddddddddddddolc;.        \n");
    twrite("     .;d0NWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW0d:.     \n");
    twrite("   .:xXWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWXx:.   \n");
    twrite("  .cONWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWNOc.  \n");
    twrite("  ;kNWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWNk;  \n");
    twrite(" ,OWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW0, \n");
    twrite(";KWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWK; \n");
    twrite("0WWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW0 \n");
    twrite("XWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWX \n");
    twrite("WWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW \n");
    twrite("WWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW \n");
    twrite("XWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWX \n");
    twrite("0WWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW0 \n");
    twrite(";KWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWK; \n");
    twrite(" ,OWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW0, \n");
    twrite("  ;kNWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWNk;  \n");
    twrite("  .cONWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWNOc.  \n");
    twrite("   .:xXWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWXx:.   \n");
    twrite("     .;d0NWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW0d:.     \n");
    twrite("       .,:lodddddddddddddddddddddddddddddddolc;.        \n");
    
    tsetcolor(COLOR_GREEN);
    twrite("\n");
    twrite("  OS: CardboardOS v2026.8 'Eclipse'\n");
    twrite("  Shell: BRASH v");
    twrite(SHELL_VERSION);
    twrite(" (POSIX Compliant)\n");
    twrite("  Kernel: Multiboot 0.95\n");
    twrite("  Architecture: i386 (Protected Mode)\n");
    twrite("  Terminal: VGA Text Mode 80x25\n");
    twrite("  CPU: x86 (32-bit)\n");
    twrite("  Memory: 16MB (Simulated)\n");
    twrite("  Uptime: 00:00:01 (Simulated)\n");
    twrite("  User: root\n");
    twrite("  Shell Prompt: BRASH>\n");
    twrite("  Theme: Default\n");
    twrite("  Packages: 0 (Custom Kernel)\n");
    twrite("  Resolution: 80x25 Text Mode\n");
    tsetcolor(COLOR_YELLOW);
    twrite("  ██████╗ █████╗ ██████╗ ██████╗ ██████╗  █████╗ ██████╗ \n");
    twrite("  ██╔══██╗██╔══██╗██╔══██╗██╔══██╗██╔══██╗██╔══██╗██╔══██╗\n");
    twrite("  ██████╔╝███████║██████╔╝██║  ██║██████╔╝███████║██████╔╝\n");
    twrite("  ██╔══██╗██╔══██║██╔══██╗██║  ██║██╔══██╗██╔══██║██╔══██╗\n");
    twrite("  ██║  ██║██║  ██║██║  ██║██████╔╝██║  ██║██║  ██║██║  ██║\n");
    twrite("  ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═╝╚═════╝ ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═╝\n");
    tsetcolor(COLOR_WHITE);
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
    twrite("  neofetch      - Display system info with ASCII art\n");
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
    outb(0x64, 0xFE);
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
    twrite("  VGA Buffer: 4000 bytes\n");
    twrite("  Stack: 4KB (Optimized)\n");
    twrite("  Kernel Base: 0x100000\n");
    twrite("  Text: 0x100000\n");
    twrite("  Data: 0x101000\n");
    twrite("  BSS: 0x102000\n");
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
    char* args[MAX_ARGS];
    int arg_count = 0;
    char* token = cmd;
    
    while (*token == ' ') token++;
    
    while (*token && arg_count < MAX_ARGS) {
        args[arg_count] = token;
        arg_count++;
        
        while (*token && *token != ' ') token++;
        
        if (*token) {
            *token = '\0';
            token++;
            while (*token == ' ') token++;
        }
    }
    
    if (arg_count == 0) {
        return;
    }
    
    // Command execution
    if (strcmp(args[0], "help") == 0 || strcmp(args[0], "?") == 0) {
        shell_help();
    }
    else if (strcmp(args[0], "echo") == 0) {
        if (arg_count > 1) {
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
    else if (strcmp(args[0], "neofetch") == 0 || strcmp(args[0], "nf") == 0) {
        shell_neofetch();
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
    
    tinit();
    shell_init();
    
    while (1) {
        shell_prompt();
        tread(cmd, MAX_CMD_LEN);
        shell_execute(cmd);
    }
}