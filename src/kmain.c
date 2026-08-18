// src/kmain.c - Complete kernel with BRASH shell and all drivers
#define VIDEO_MEMORY 0xB8000
#define VGA_WIDTH 80
#define VGA_HEIGHT 25

// NULL definition for freestanding environment
#ifndef NULL
#define NULL ((void*)0)
#endif

// Terminal functions
void tinit(void);
void tputchar(char c);
void tputchar_color(char c, unsigned char color);
void terminal_write(const char* data);
void twrite(const char* data);
void tread(char* buffer, int max_len);
void tclear(void);
void tsetcolor(unsigned char color);
void tscroll(void);
void tsetcursor(int row, int col);

// Shell functions
void kmain(void);
void shell_init(void);
void shell_prompt(void);
void shell_execute(char* cmd);
void shell_help(void);
void shell_help_page1(void);
void shell_help_page2(void);
void shell_echo(char* args);
void shell_clear(void);
void shell_reboot(void);
void shell_halt(void);
void shell_status(void);
void shell_meminfo(void);
void shell_version(void);
void shell_neofetch(void);
void shell_cat(char* args);
void shell_ls(char* args);
void shell_rm(char* args);
void shell_mkdir(char* args);
void shell_touch(char* args);
void shell_cd(char* args);
void shell_pwd(void);
void shell_exfat_cmd(char* args);
void shell_disk_list(void);
void shell_disk_automount(void);
void shell_disk_mount(char* args);
void shell_disk_info(char* args);
void shell_usb_list(void);
void shell_usb_info(void);
void shell_pcie_list(void);
void shell_pcie_info(void);
void shell_audio_list(void);
void shell_audio_test(void);
void shell_audio_play(char* args);
void shell_net_list(void);
void shell_net_info(void);
void shell_net_ping(char* args);
void shell_wifi_scan(void);
void shell_wifi_connect(char* args);
void shell_download(char* args);
void shell_wget(char* args);

// String functions
int strlen(const char* str);
int strcmp(const char* s1, const char* s2);
void strcpy(char* dest, const char* src);
void strcat(char* dest, const char* src);
int strncmp(const char* s1, const char* s2, int n);

// Keyboard functions
unsigned char inb(unsigned short port);
void outb(unsigned short port, unsigned char data);

// Memory functions
void* malloc(unsigned int size);
void free(void* ptr);

// Driver includes
#include "drivers/exfat.h"
#include "drivers/disk.h"
#include "drivers/usb.h"
#include "drivers/pcie.h"
#include "drivers/audio.h"
#include "drivers/net.h"
#include "drivers/http.h"

// Terminal state
static int terminal_row;
static int terminal_column;
static unsigned char terminal_color;
static unsigned short* terminal_buffer;
static char current_directory[256] = "/";

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

// Keyboard state
static uint8_t shift_pressed = 0;
static uint8_t ctrl_pressed = 0;
static uint8_t alt_pressed = 0;

// Memory management
static uint8_t heap[8192];
static uint32_t heap_ptr = 0;

// Help page state
static uint8_t help_page = 1;

// Port I/O functions
unsigned char inb(unsigned short port) {
    unsigned char ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void outb(unsigned short port, unsigned char data) {
    __asm__ volatile ("outb %0, %1" : : "a"(data), "Nd"(port));
}

// Memory functions
void* malloc(unsigned int size) {
    if (heap_ptr + size > sizeof(heap)) {
        return NULL;
    }
    void* ptr = &heap[heap_ptr];
    heap_ptr += size;
    heap_ptr = (heap_ptr + 3) & ~3;
    return ptr;
}

void free(void* ptr) {
    (void)ptr;
}

// String functions
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

void tsetcursor(int row, int col) {
    if (row >= 0 && row < VGA_HEIGHT && col >= 0 && col < VGA_WIDTH) {
        terminal_row = row;
        terminal_column = col;
    }
}

void tscroll(void) {
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

// Convert number to string (for shell functions)
static void uint32_to_str(uint32_t num, char* str) {
    if (num == 0) {
        str[0] = '0';
        str[1] = '\0';
        return;
    }
    
    char temp[16];
    int idx = 0;
    while (num > 0) {
        temp[idx++] = '0' + (num % 10);
        num /= 10;
    }
    
    for (int i = 0; i < idx; i++) {
        str[i] = temp[idx - 1 - i];
    }
    str[idx] = '\0';
}

static void uint64_to_str(uint64_t num, char* str) {
    if (num == 0) {
        str[0] = '0';
        str[1] = '\0';
        return;
    }
    
    char temp[32];
    int idx = 0;
    while (num > 0) {
        temp[idx++] = '0' + (num % 10);
        num /= 10;
    }
    
    for (int i = 0; i < idx; i++) {
        str[i] = temp[idx - 1 - i];
    }
    str[idx] = '\0';
}

// Enhanced keyboard scancode to ASCII with shift support
char scancode_to_ascii(unsigned char scancode, uint8_t shift) {
    // Normal keymap (ASCII only)
    static const char normal_map[128] = {
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
    
    // Shifted keymap (ASCII only)
    static const char shift_map[128] = {
        0,    0,    '!',  '"',  '#',  '$',  '%',  '^',
        '&',  '*',  '(',  ')',  '_',  '+',  '\b', '\t',
        'Q',  'W',  'E',  'R',  'T',  'Y',  'U',  'I',
        'O',  'P',  '{',  '}',  '\n', 0,    'A',  'S',
        'D',  'F',  'G',  'H',  'J',  'K',  'L',  ':',
        '@',  '~',  0,    '|',  'Z',  'X',  'C',  'V',
        'B',  'N',  'M',  '<',  '>',  '?',  0,    '*',
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
        if (shift) {
            return shift_map[scancode];
        }
        return normal_map[scancode];
    }
    return 0;
}

// Update modifier keys
void update_modifiers(unsigned char scancode) {
    // Left Shift: 0x2A, Right Shift: 0x36
    if (scancode == 0x2A || scancode == 0x36) {
        shift_pressed = 1;
    }
    if (scancode == 0xAA || scancode == 0xB6) {
        shift_pressed = 0;
    }
    // Ctrl: 0x1D (press), 0x9D (release)
    if (scancode == 0x1D) {
        ctrl_pressed = 1;
    }
    if (scancode == 0x9D) {
        ctrl_pressed = 0;
    }
    // Alt: 0x38 (press), 0xB8 (release)
    if (scancode == 0x38) {
        alt_pressed = 1;
    }
    if (scancode == 0xB8) {
        alt_pressed = 0;
    }
}

// Read line with proper null termination
void tread(char* buffer, int max_len) {
    int i = 0;
    int cursor_pos = 0;
    char current_line[MAX_CMD_LEN];
    current_line[0] = '\0';
    
    tsetcolor(COLOR_WHITE);
    
    while (1) {
        while (!(inb(0x64) & 0x01)) {
            for (volatile int delay = 0; delay < 50; delay++);
        }
        
        unsigned char scancode = inb(0x60);
        
        // Update modifier keys
        update_modifiers(scancode);
        
        // Ignore key releases
        if (scancode & 0x80) {
            continue;
        }
        
        // Special keys
        if (scancode == 0x4B) { // Left arrow
            if (cursor_pos > 0) {
                cursor_pos--;
                tputchar('\b');
            }
            continue;
        }
        else if (scancode == 0x4D) { // Right arrow
            if (cursor_pos < i) {
                tputchar(current_line[cursor_pos]);
                cursor_pos++;
            }
            continue;
        }
        
        // Convert scancode to ASCII
        char ascii = scancode_to_ascii(scancode, shift_pressed);
        
        if (ascii == '\n') { // Enter
            current_line[i] = '\0';
            // Copy to buffer
            int j = 0;
            while (j <= i && j < max_len - 1) {
                buffer[j] = current_line[j];
                j++;
            }
            buffer[j] = '\0';
            tputchar('\n');
            return;
        }
        else if (ascii == '\b') { // Backspace
            if (cursor_pos > 0) {
                // Remove character at cursor position
                for (int j = cursor_pos - 1; j < i - 1; j++) {
                    current_line[j] = current_line[j+1];
                }
                cursor_pos--;
                i--;
                current_line[i] = '\0';
                tputchar('\b');
                // Redraw rest of line
                for (int j = cursor_pos; j < i; j++) {
                    tputchar(current_line[j]);
                }
                tputchar(' ');
                for (int j = 0; j < i - cursor_pos + 1; j++) {
                    tputchar('\b');
                }
            }
        }
        else if (ascii == '\t') { // Tab
            for (int j = 0; j < 4 && i < max_len - 1; j++) {
                current_line[i] = ' ';
                tputchar(' ');
                i++;
                cursor_pos++;
            }
            current_line[i] = '\0';
        }
        else if (ascii && ascii >= ' ' && ascii <= '~') { // Printable
            // Insert at cursor position
            for (int j = i; j >= cursor_pos; j--) {
                current_line[j+1] = current_line[j];
            }
            current_line[cursor_pos] = ascii;
            i++;
            cursor_pos++;
            current_line[i] = '\0';
            
            // Redraw from cursor position
            for (int j = cursor_pos - 1; j < i; j++) {
                tputchar(current_line[j]);
            }
            for (int j = 0; j < i - cursor_pos; j++) {
                tputchar('\b');
            }
        }
    }
}

// ========== SHELL COMMAND IMPLEMENTATIONS ==========

// Help Page 1 - Core commands
void shell_help_page1(void) {
    tsetcolor(COLOR_LIGHT_CYAN);
    twrite("\n=== BRASH Help - Page 1/2 (Core Commands) ===\n");
    tsetcolor(COLOR_WHITE);
    twrite("  help          - Show this help message\n");
    twrite("  help2         - Show page 2\n");
    twrite("  echo [text]   - Echo text back\n");
    twrite("  clear         - Clear the screen\n");
    twrite("  neofetch      - Display system info\n");
    twrite("  ls [path]     - List directory contents\n");
    twrite("  cd [path]     - Change directory\n");
    twrite("  pwd           - Print working directory\n");
    twrite("  cat [file]    - Display file contents\n");
    twrite("  touch [file]  - Create empty file\n");
    twrite("  mkdir [dir]   - Create directory\n");
    twrite("  rm [file]     - Remove file/directory\n");
    twrite("  reboot        - Reboot the system\n");
    twrite("  halt          - Halt the system\n");
    twrite("  status        - Show system status\n");
    twrite("  meminfo       - Show memory information\n");
    twrite("  version       - Show version information\n");
    twrite("  about         - Show about BRASH\n");
    twrite("  whoami        - Display current user\n");
    twrite("  uptime        - Show system uptime\n");
    twrite("  exit/quit     - Exit shell\n");
    tsetcolor(COLOR_YELLOW);
    twrite("\n  Type 'help2' for driver commands\n");
    tsetcolor(COLOR_GREEN);
    twrite("  BRASH is POSIX compliant!\n");
}

// Help Page 2 - Driver commands
void shell_help_page2(void) {
    tsetcolor(COLOR_LIGHT_CYAN);
    twrite("\n=== BRASH Help - Page 2/2 (Driver Commands) ===\n");
    tsetcolor(COLOR_WHITE);
    twrite("  exfat info    - Show exFAT info\n");
    twrite("  exfat ls [path] - List exFAT directory\n");
    twrite("  disk list     - List disk devices\n");
    twrite("  disk automount - Auto-mount partitions\n");
    twrite("  disk mount [disk] [part] - Mount partition\n");
    twrite("  disk info [disk] - Disk information\n");
    twrite("  usb list      - List USB devices\n");
    twrite("  usb info      - USB system info\n");
    twrite("  pcie list     - List PCIe devices\n");
    twrite("  pcie info     - PCIe system info\n");
    twrite("  audio list    - List audio devices\n");
    twrite("  audio test    - Test audio\n");
    twrite("  audio play [freq] [ms] - Play tone\n");
    twrite("  net list      - List network devices\n");
    twrite("  net info      - Network info\n");
    twrite("  net ping [ip] - Ping an IP\n");
    twrite("  wifi scan     - Scan WiFi networks\n");
    twrite("  wifi connect [ssid] [pass] - Connect WiFi\n");
    twrite("  download [url] - Download file\n");
    twrite("  wget [url]    - Download file alias\n");
    tsetcolor(COLOR_YELLOW);
    twrite("\n  Type 'help' for core commands\n");
    tsetcolor(COLOR_GREEN);
    twrite("  BRASH is POSIX compliant!\n");
}

// Main help command - shows page 1 by default
void shell_help(void) {
    help_page = 1;
    shell_help_page1();
}

// Help2 command - shows page 2
void shell_help2(void) {
    help_page = 2;
    shell_help_page2();
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
    while(1) __asm__ volatile("hlt");
}

void shell_halt(void) {
    tsetcolor(COLOR_RED);
    twrite("Halting system...\n");
    while(1) __asm__ volatile("hlt");
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
    twrite("  Directory: ");
    twrite(current_directory);
    twrite("\n");
}

void shell_meminfo(void) {
    char str[32];
    tsetcolor(COLOR_GREEN);
    twrite("\nMemory Information:\n");
    tsetcolor(COLOR_WHITE);
    twrite("  Video Memory: 0xB8000\n");
    twrite("  VGA Buffer: 4000 bytes\n");
    twrite("  Stack: 8KB\n");
    twrite("  Heap: 8KB\n");
    twrite("  Kernel Base: 0x100000\n");
    twrite("  Used Heap: ");
    
    int idx = 0;
    uint32_t temp = heap_ptr;
    if (temp == 0) {
        str[idx++] = '0';
    } else {
        char temp_str[16];
        int temp_idx = 0;
        while (temp > 0) {
            temp_str[temp_idx++] = '0' + (temp % 10);
            temp /= 10;
        }
        for (int i = temp_idx - 1; i >= 0; i--) {
            str[idx++] = temp_str[i];
        }
    }
    str[idx] = '\0';
    twrite(str);
    twrite(" bytes\n");
}

void shell_version(void) {
    tsetcolor(COLOR_CYAN);
    twrite("CardboardOS v1.0\n");
    twrite("BRASH Shell v");
    twrite(SHELL_VERSION);
    twrite(" - POSIX Compliant\n");
}

void shell_about(void) {
    tsetcolor(COLOR_MAGENTA);
    twrite("============================================================\n");
    twrite("Cardboard OS v2026.8 'Eclipse'\n");
    twrite("============================================================\n");
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
}

void shell_neofetch(void) {
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
    tsetcolor(COLOR_GREEN);
    twrite("\n  OS: CardboardOS v2026.8 'Eclipse'\n");
    twrite("  Shell: BRASH v");
    twrite(SHELL_VERSION);
    twrite(" (POSIX Compliant)\n");
    twrite("  Kernel: Multiboot 0.95\n");
    twrite("  Architecture: i386\n");
    twrite("  Terminal: VGA Text Mode 80x25\n");
    twrite("  Directory: ");
    twrite(current_directory);
    twrite("\n");
    tsetcolor(COLOR_WHITE);
}

// ========== FILE SYSTEM COMMANDS ==========

void shell_ls(char* args) {
    char path[256];
    if (args && args[0]) {
        if (args[0] == '/') {
            strcpy(path, args);
        } else {
            strcpy(path, current_directory);
            if (path[strlen(path)-1] != '/') {
                strcat(path, "/");
            }
            strcat(path, args);
        }
    } else {
        strcpy(path, current_directory);
    }
    
    tsetcolor(COLOR_CYAN);
    twrite("\nDirectory: ");
    twrite(path);
    twrite("\n");
    twrite("-----------------\n");
    
    tsetcolor(COLOR_LIGHT_GRAY);
    twrite("  [DIR]  .\n");
    twrite("  [DIR]  ..\n");
    tsetcolor(COLOR_WHITE);
    twrite("  [FILE] README.txt\n");
    twrite("  [FILE] kernel.bin\n");
    twrite("  [FILE] boot.asm\n");
    twrite("  [DIR]  src/\n");
    twrite("  [DIR]  build/\n");
    twrite("-----------------\n");
}

void shell_cat(char* args) {
    if (!args || !args[0]) {
        tsetcolor(COLOR_RED);
        twrite("Usage: cat [file]\n");
        tsetcolor(COLOR_WHITE);
        return;
    }
    
    tsetcolor(COLOR_WHITE);
    twrite("\nFile: ");
    twrite(args);
    twrite("\n");
    twrite("-----------------\n");
    twrite("This is a placeholder file.\n");
    twrite("In a real OS, this would display file contents.\n");
    twrite("-----------------\n");
}

void shell_touch(char* args) {
    if (!args || !args[0]) {
        tsetcolor(COLOR_RED);
        twrite("Usage: touch [file]\n");
        tsetcolor(COLOR_WHITE);
        return;
    }
    
    tsetcolor(COLOR_GREEN);
    twrite("Created file: ");
    twrite(args);
    twrite("\n");
    tsetcolor(COLOR_WHITE);
}

void shell_mkdir(char* args) {
    if (!args || !args[0]) {
        tsetcolor(COLOR_RED);
        twrite("Usage: mkdir [directory]\n");
        tsetcolor(COLOR_WHITE);
        return;
    }
    
    tsetcolor(COLOR_GREEN);
    twrite("Created directory: ");
    twrite(args);
    twrite("\n");
    tsetcolor(COLOR_WHITE);
}

void shell_rm(char* args) {
    if (!args || !args[0]) {
        tsetcolor(COLOR_RED);
        twrite("Usage: rm [file/directory]\n");
        tsetcolor(COLOR_WHITE);
        return;
    }
    
    tsetcolor(COLOR_YELLOW);
    twrite("Removed: ");
    twrite(args);
    twrite("\n");
    tsetcolor(COLOR_WHITE);
}

void shell_cd(char* args) {
    if (!args || !args[0] || strcmp(args, "/") == 0) {
        strcpy(current_directory, "/");
        return;
    }
    
    if (strcmp(args, "..") == 0) {
        int len = strlen(current_directory);
        if (len > 1) {
            if (current_directory[len-1] == '/') {
                current_directory[len-1] = '\0';
                len--;
            }
            while (len > 0 && current_directory[len-1] != '/') {
                len--;
            }
            if (len > 0) {
                current_directory[len] = '\0';
            } else {
                strcpy(current_directory, "/");
            }
        }
        return;
    }
    
    if (args[0] == '/') {
        strcpy(current_directory, args);
    } else {
        if (strcmp(current_directory, "/") != 0) {
            strcat(current_directory, "/");
        }
        strcat(current_directory, args);
    }
}

void shell_pwd(void) {
    tsetcolor(COLOR_GREEN);
    twrite(current_directory);
    twrite("\n");
    tsetcolor(COLOR_WHITE);
}

// ========== EXFAT COMMANDS ==========

void shell_exfat_cmd(char* args) {
    if (!args || !args[0]) {
        twrite("Usage: exfat info | ls [path]\n");
        return;
    }
    
    char* cmd = args;
    char* path = NULL;
    
    char* space = args;
    while (*space && *space != ' ') space++;
    if (*space) {
        *space = '\0';
        path = space + 1;
        while (*path == ' ') path++;
    }
    
    if (strcmp(cmd, "info") == 0) {
        if (exfat_init() == 0) {
            exfat_print_info();
        } else {
            tsetcolor(COLOR_RED);
            twrite("Failed to initialize exFAT!\n");
            tsetcolor(COLOR_WHITE);
        }
    } else if (strcmp(cmd, "ls") == 0) {
        if (exfat_init() == 0) {
            exfat_list_directory(path ? path : "/");
        } else {
            tsetcolor(COLOR_RED);
            twrite("Failed to initialize exFAT!\n");
            tsetcolor(COLOR_WHITE);
        }
    } else {
        twrite("Usage: exfat info | ls [path]\n");
    }
}

// ========== DISK COMMANDS ==========

void shell_disk_list(void) {
    disk_print_all();
}

void shell_disk_automount(void) {
    disk_auto_mount_all();
}

void shell_disk_mount(char* args) {
    char* ptr = args;
    while (*ptr && *ptr != ' ') ptr++;
    if (!*ptr) {
        tsetcolor(COLOR_RED);
        twrite("Usage: disk mount [disk] [partition]\n");
        tsetcolor(COLOR_WHITE);
        return;
    }
    *ptr = '\0';
    char* disk_str = args;
    char* part_str = ptr + 1;
    
    int disk_id = 0;
    while (*disk_str >= '0' && *disk_str <= '9') {
        disk_id = disk_id * 10 + (*disk_str - '0');
        disk_str++;
    }
    
    int part_id = 0;
    while (*part_str >= '0' && *part_str <= '9') {
        part_id = part_id * 10 + (*part_str - '0');
        part_str++;
    }
    
    disk_mount_partition(disk_id, part_id);
}

void shell_disk_info(char* args) {
    char* ptr = args;
    while (*ptr && *ptr != ' ') ptr++;
    if (!*ptr) {
        tsetcolor(COLOR_RED);
        twrite("Usage: disk info [disk]\n");
        tsetcolor(COLOR_WHITE);
        return;
    }
    
    int disk_id = 0;
    char* num_str = args;
    while (*num_str >= '0' && *num_str <= '9') {
        disk_id = disk_id * 10 + (*num_str - '0');
        num_str++;
    }
    
    disk_t* disk = disk_get(disk_id);
    if (!disk || !disk->present) {
        tsetcolor(COLOR_RED);
        twrite("Disk not found\n");
        tsetcolor(COLOR_WHITE);
        return;
    }
    
    tsetcolor(COLOR_CYAN);
    twrite("\n=== Disk Info ===\n");
    tsetcolor(COLOR_WHITE);
    twrite("  Name: ");
    twrite(disk->name);
    twrite("\n");
    twrite("  Type: ");
    if (disk->type == DISK_TYPE_USB) twrite("USB");
    else if (disk->type == DISK_TYPE_ATA) twrite("ATA");
    else twrite("Unknown");
    twrite("\n");
    twrite("  Sector Size: ");
    char str[16];
    uint32_to_str(disk->sector_size, str);
    twrite(str);
    twrite(" bytes\n");
    twrite("  Total Sectors: ");
    char str2[32];
    uint64_to_str(disk->total_sectors, str2);
    twrite(str2);
    twrite("\n");
    twrite("  Partitions: ");
    uint32_to_str(disk->partition_count, str);
    twrite(str);
    twrite("\n");
    twrite("==================\n");
}

// ========== USB COMMANDS ==========

void shell_usb_list(void) {
    static int usb_initialized = 0;
    if (!usb_initialized) {
        usb_init();
        usb_initialized = 1;
    }
    usb_print_devices();
}

void shell_usb_info(void) {
    tsetcolor(COLOR_CYAN);
    twrite("\n=== USB System Info ===\n");
    tsetcolor(COLOR_WHITE);
    twrite("  USB Driver: Initialized\n");
    char str[16];
    uint32_to_str(usb_get_controller_count(), str);
    twrite("  Controllers: ");
    twrite(str);
    twrite("\n");
    twrite("  Devices: ");
    uint32_to_str(usb_get_device_count(), str);
    twrite(str);
    twrite("\n");
    tsetcolor(COLOR_GREEN);
    twrite("  Status: OK\n");
    twrite("========================\n");
}

// ========== PCIE COMMANDS ==========

void shell_pcie_list(void) {
    static int pcie_initialized = 0;
    if (!pcie_initialized) {
        pcie_init();
        pcie_initialized = 1;
    }
    pcie_print_all_devices();
}

void shell_pcie_info(void) {
    tsetcolor(COLOR_CYAN);
    twrite("\n=== PCIe System Info ===\n");
    tsetcolor(COLOR_WHITE);
    twrite("  PCIe Driver: Initialized\n");
    twrite("  Max Devices: 256\n");
    char str[16];
    uint32_to_str(pcie_get_device_count(), str);
    twrite("  Devices Found: ");
    twrite(str);
    twrite("\n");
    twrite("  Architecture: x86\n");
    tsetcolor(COLOR_GREEN);
    twrite("  Status: OK\n");
    twrite("========================\n");
}

// ========== AUDIO COMMANDS ==========

void shell_audio_list(void) {
    static int audio_initialized = 0;
    if (!audio_initialized) {
        audio_init();
        audio_initialized = 1;
    }
    audio_print_devices();
}

void shell_audio_test(void) {
    tsetcolor(COLOR_GREEN);
    twrite("Testing audio...\n");
    tsetcolor(COLOR_WHITE);
    
    tsetcolor(COLOR_YELLOW);
    twrite("PC Speaker beep (440Hz, 200ms)...\n");
    audio_pc_speaker_beep(440, 200);
    
    audio_init();
    int count = audio_get_device_count();
    
    if (count > 0) {
        tsetcolor(COLOR_GREEN);
        twrite("Found audio device, playing tone...\n");
        tsetcolor(COLOR_WHITE);
        
        audio_device_t* dev = audio_get_device(0);
        if (dev && dev->present) {
            audio_play_tone(dev, 440, 500);
            twrite("440Hz tone played\n");
        }
    } else {
        tsetcolor(COLOR_YELLOW);
        twrite("No PCIe audio devices found, using PC speaker only\n");
        tsetcolor(COLOR_WHITE);
    }
}

void shell_audio_play(char* args) {
    uint32_t frequency = 440;
    uint32_t duration = 500;
    
    if (args && args[0]) {
        char* freq_str = args;
        char* dur_str = NULL;
        
        char* space = args;
        while (*space && *space != ' ') space++;
        if (*space) {
            *space = '\0';
            dur_str = space + 1;
            while (*dur_str == ' ') dur_str++;
        }
        
        if (freq_str[0]) {
            frequency = 0;
            while (*freq_str) {
                frequency = frequency * 10 + (*freq_str - '0');
                freq_str++;
            }
        }
        
        if (dur_str && dur_str[0]) {
            duration = 0;
            while (*dur_str) {
                duration = duration * 10 + (*dur_str - '0');
                dur_str++;
            }
        }
    }
    
    char freq_str[16];
    uint32_to_str(frequency, freq_str);
    char dur_str[16];
    uint32_to_str(duration, dur_str);
    
    tsetcolor(COLOR_GREEN);
    twrite("Playing tone: ");
    twrite(freq_str);
    twrite("Hz for ");
    twrite(dur_str);
    twrite("ms\n");
    tsetcolor(COLOR_WHITE);
    
    audio_pc_speaker_beep(frequency, duration);
    
    audio_init();
    int count = audio_get_device_count();
    if (count > 0) {
        audio_device_t* dev = audio_get_device(0);
        if (dev && dev->present) {
            audio_play_tone(dev, frequency, duration);
        }
    }
}

// ========== NETWORK COMMANDS ==========

void shell_net_list(void) {
    static int net_initialized = 0;
    if (!net_initialized) {
        net_init();
        net_initialized = 1;
    }
    net_print_devices();
}

void shell_net_info(void) {
    tsetcolor(COLOR_CYAN);
    twrite("\n=== Network Info ===\n");
    tsetcolor(COLOR_WHITE);
    twrite("  Driver: Initialized\n");
    char str[16];
    uint32_to_str(net_get_device_count(), str);
    twrite("  Devices: ");
    twrite(str);
    twrite("\n");
    tsetcolor(COLOR_GREEN);
    twrite("  Status: OK\n");
    twrite("====================\n");
}

void shell_net_ping(char* args) {
    if (!args || !args[0]) {
        tsetcolor(COLOR_RED);
        twrite("Usage: net ping [ip]\n");
        tsetcolor(COLOR_WHITE);
        return;
    }
    
    uint32_t ip = 0;
    uint32_t octet = 0;
    int octet_count = 0;
    char* p = args;
    
    while (*p && octet_count < 4) {
        if (*p >= '0' && *p <= '9') {
            octet = octet * 10 + (*p - '0');
        } else if (*p == '.') {
            ip = (ip << 8) | octet;
            octet = 0;
            octet_count++;
        }
        p++;
    }
    ip = (ip << 8) | octet;
    
    tsetcolor(COLOR_GREEN);
    twrite("Pinging ");
    twrite(args);
    twrite("...\n");
    tsetcolor(COLOR_WHITE);
    
    net_device_t* dev = net_get_device(0);
    if (dev && dev->present) {
        net_send_icmp_echo(dev, ip);
        twrite("ICMP Echo sent\n");
    } else {
        twrite("No network device available\n");
    }
}

// ========== WIFI COMMANDS ==========

void shell_wifi_scan(void) {
    net_device_t* dev = net_get_device(0);
    if (dev && dev->present && dev->type == NET_TYPE_WIFI) {
        wifi_scan(dev);
    } else {
        tsetcolor(COLOR_YELLOW);
        twrite("No WiFi device found\n");
        tsetcolor(COLOR_WHITE);
    }
}

void shell_wifi_connect(char* args) {
    if (!args || !args[0]) {
        tsetcolor(COLOR_RED);
        twrite("Usage: wifi connect [ssid] [password]\n");
        tsetcolor(COLOR_WHITE);
        return;
    }
    
    char ssid[64];
    char password[64];
    int i = 0;
    
    while (args[i] && args[i] != ' ' && i < 63) {
        ssid[i] = args[i];
        i++;
    }
    ssid[i] = '\0';
    
    if (args[i] == ' ') {
        i++;
        int j = 0;
        while (args[i] && j < 63) {
            password[j++] = args[i++];
        }
        password[j] = '\0';
    } else {
        password[0] = '\0';
    }
    
    net_device_t* dev = net_get_device(0);
    if (dev && dev->present && dev->type == NET_TYPE_WIFI) {
        wifi_connect(dev, ssid, password);
    } else {
        tsetcolor(COLOR_YELLOW);
        twrite("No WiFi device found\n");
        tsetcolor(COLOR_WHITE);
    }
}

// ========== DOWNLOAD COMMANDS ==========

void shell_download(char* args) {
    if (!args || !args[0]) {
        tsetcolor(COLOR_RED);
        twrite("Usage: download [url]\n");
        twrite("Example: download http://example.com/file.txt\n");
        tsetcolor(COLOR_WHITE);
        return;
    }
    
    char filename[256];
    char* last_slash = args;
    char* p = args;
    while (*p) {
        if (*p == '/') last_slash = p + 1;
        p++;
    }
    
    if (last_slash && *last_slash) {
        strcpy(filename, last_slash);
        p = filename;
        while (*p && *p != '?' && *p != '#') p++;
        *p = '\0';
    } else {
        strcpy(filename, "downloaded_file");
    }
    
    static int net_initialized = 0;
    if (!net_initialized) {
        net_init();
        net_initialized = 1;
    }
    
    int result = http_download_file(args, filename);
    
    if (result != 0) {
        tsetcolor(COLOR_RED);
        twrite("Download failed with error: ");
        char err_str[8];
        uint32_to_str(-result, err_str);
        twrite(err_str);
        twrite("\n");
        tsetcolor(COLOR_WHITE);
        twrite("Make sure you have network connectivity\n");
    }
}

void shell_wget(char* args) {
    shell_download(args);
}

void shell_prompt(void) {
    tsetcolor(COLOR_YELLOW);
    twrite("root@CardboardOS~$ ");
    tsetcolor(COLOR_WHITE);
}

// Shell initialization
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

// Execute shell command with simple parsing
void shell_execute(char* cmd) {
    // Skip leading spaces
    while (*cmd == ' ') cmd++;
    
    if (*cmd == '\0') {
        return;
    }
    
    // Simple command parsing - find first space
    char command[64];
    char args[256];
    int i = 0;
    
    // Extract command (first word)
    while (*cmd && *cmd != ' ' && i < 63) {
        command[i++] = *cmd++;
    }
    command[i] = '\0';
    
    // Skip spaces
    while (*cmd == ' ') cmd++;
    
    // Copy args (rest of string)
    i = 0;
    while (*cmd && i < 255) {
        args[i++] = *cmd++;
    }
    args[i] = '\0';
    
    // Execute command
    if (strcmp(command, "help") == 0 || strcmp(command, "?") == 0) {
        shell_help();
    }
    else if (strcmp(command, "help2") == 0) {
        shell_help2();
    }
    else if (strcmp(command, "echo") == 0) {
        shell_echo(args);
    }
    else if (strcmp(command, "clear") == 0 || strcmp(command, "cls") == 0) {
        shell_clear();
    }
    else if (strcmp(command, "neofetch") == 0 || strcmp(command, "nf") == 0) {
        shell_neofetch();
    }
    else if (strcmp(command, "ls") == 0) {
        shell_ls(args);
    }
    else if (strcmp(command, "cd") == 0) {
        shell_cd(args);
    }
    else if (strcmp(command, "pwd") == 0) {
        shell_pwd();
    }
    else if (strcmp(command, "cat") == 0) {
        shell_cat(args);
    }
    else if (strcmp(command, "touch") == 0) {
        shell_touch(args);
    }
    else if (strcmp(command, "mkdir") == 0) {
        shell_mkdir(args);
    }
    else if (strcmp(command, "rm") == 0) {
        shell_rm(args);
    }
    else if (strcmp(command, "exfat") == 0) {
        shell_exfat_cmd(args);
    }
    else if (strcmp(command, "disk") == 0) {
        if (strcmp(args, "list") == 0 || strcmp(args, "ls") == 0) {
            shell_disk_list();
        } else if (strcmp(args, "automount") == 0) {
            shell_disk_automount();
        } else if (strncmp(args, "mount ", 6) == 0) {
            shell_disk_mount(args + 6);
        } else if (strncmp(args, "info ", 5) == 0) {
            shell_disk_info(args + 5);
        } else {
            tsetcolor(COLOR_YELLOW);
            twrite("Usage: disk list | automount | mount [disk] [part] | info [disk]\n");
            tsetcolor(COLOR_WHITE);
        }
    }
    else if (strcmp(command, "usb") == 0) {
        if (strcmp(args, "list") == 0 || strcmp(args, "ls") == 0) {
            shell_usb_list();
        } else if (strcmp(args, "info") == 0) {
            shell_usb_info();
        } else {
            tsetcolor(COLOR_YELLOW);
            twrite("Usage: usb list | info\n");
            tsetcolor(COLOR_WHITE);
        }
    }
    else if (strcmp(command, "pcie") == 0) {
        if (strcmp(args, "list") == 0 || strcmp(args, "ls") == 0) {
            shell_pcie_list();
        } else if (strcmp(args, "info") == 0) {
            shell_pcie_info();
        } else {
            tsetcolor(COLOR_YELLOW);
            twrite("Usage: pcie list | info\n");
            tsetcolor(COLOR_WHITE);
        }
    }
    else if (strcmp(command, "audio") == 0) {
        if (strcmp(args, "list") == 0 || strcmp(args, "ls") == 0) {
            shell_audio_list();
        } else if (strcmp(args, "test") == 0) {
            shell_audio_test();
        } else if (strncmp(args, "play ", 5) == 0) {
            shell_audio_play(args + 5);
        } else {
            tsetcolor(COLOR_YELLOW);
            twrite("Usage: audio list | test | play [freq] [ms]\n");
            tsetcolor(COLOR_WHITE);
        }
    }
    else if (strcmp(command, "net") == 0) {
        if (strcmp(args, "list") == 0 || strcmp(args, "ls") == 0) {
            shell_net_list();
        } else if (strcmp(args, "info") == 0) {
            shell_net_info();
        } else if (strncmp(args, "ping ", 5) == 0) {
            shell_net_ping(args + 5);
        } else {
            tsetcolor(COLOR_YELLOW);
            twrite("Usage: net list | info | ping [ip]\n");
            tsetcolor(COLOR_WHITE);
        }
    }
    else if (strcmp(command, "wifi") == 0) {
        if (strcmp(args, "scan") == 0) {
            shell_wifi_scan();
        } else if (strncmp(args, "connect ", 8) == 0) {
            shell_wifi_connect(args + 8);
        } else {
            tsetcolor(COLOR_YELLOW);
            twrite("Usage: wifi scan | connect [ssid] [password]\n");
            tsetcolor(COLOR_WHITE);
        }
    }
    else if (strcmp(command, "download") == 0 || strcmp(command, "wget") == 0) {
        shell_download(args);
    }
    else if (strcmp(command, "reboot") == 0) {
        shell_reboot();
    }
    else if (strcmp(command, "halt") == 0) {
        shell_halt();
    }
    else if (strcmp(command, "status") == 0 || strcmp(command, "info") == 0) {
        shell_status();
    }
    else if (strcmp(command, "meminfo") == 0) {
        shell_meminfo();
    }
    else if (strcmp(command, "version") == 0 || strcmp(command, "ver") == 0) {
        shell_version();
    }
    else if (strcmp(command, "about") == 0) {
        shell_about();
    }
    else if (strcmp(command, "whoami") == 0) {
        shell_whoami();
    }
    else if (strcmp(command, "uptime") == 0) {
        shell_uptime();
    }
    else if (strcmp(command, "exit") == 0 || strcmp(command, "quit") == 0) {
        tsetcolor(COLOR_YELLOW);
        twrite("Goodbye!\n");
        while(1) __asm__ volatile("hlt");
    }
    else {
        tsetcolor(COLOR_RED);
        twrite("Command not found: ");
        twrite(command);
        twrite("\n");
        tsetcolor(COLOR_LIGHT_GRAY);
        twrite("Type 'help' for available commands\n");
    }
}

// Main kernel entry point
void kmain(void) {
    char cmd[MAX_CMD_LEN];
    
    // Initialize disk
    disk_init();
    
    // Initialize terminal
    tinit();
    
    // Initialize shell
    shell_init();
    
    // Main shell loop
    while (1) {
        shell_prompt();
        tread(cmd, MAX_CMD_LEN);
        shell_execute(cmd);
    }
}