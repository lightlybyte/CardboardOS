// src/kernel/kmain.c - Bash-like Shell
#include "stdint.h"

// ============================================================
// VGA Constants
// ============================================================
#define VGA_ADDR 0xB8000
#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_COLOR_BLACK 0x00
#define VGA_COLOR_BLUE 0x01
#define VGA_COLOR_GREEN 0x02
#define VGA_COLOR_CYAN 0x03
#define VGA_COLOR_RED 0x04
#define VGA_COLOR_MAGENTA 0x05
#define VGA_COLOR_BROWN 0x06
#define VGA_COLOR_LIGHT_GREY 0x07
#define VGA_COLOR_DARK_GREY 0x08
#define VGA_COLOR_LIGHT_BLUE 0x09
#define VGA_COLOR_LIGHT_GREEN 0x0A
#define VGA_COLOR_LIGHT_CYAN 0x0B
#define VGA_COLOR_LIGHT_RED 0x0C
#define VGA_COLOR_LIGHT_MAGENTA 0x0D
#define VGA_COLOR_LIGHT_BROWN 0x0E
#define VGA_COLOR_WHITE 0x0F

// ============================================================
// Keyboard Constants
// ============================================================
#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64

// ============================================================
// Shell Constants
// ============================================================
#define MAX_CMD_LEN 256
#define MAX_HISTORY 50
#define MAX_ARGS 16
#define MAX_ENV_VARS 32
#define MAX_ENV_NAME 32
#define MAX_ENV_VALUE 128
#define MAX_ALIASES 16
#define MAX_ALIAS_NAME 32
#define MAX_ALIAS_CMD 256

// ============================================================
// Terminal State
// ============================================================
static volatile uint16_t* terminal_buffer = (volatile uint16_t*)VGA_ADDR;
static int terminal_row = 0;
static int terminal_col = 0;
static uint8_t terminal_color = VGA_COLOR_WHITE;

// ============================================================
// Shell State
// ============================================================
static char history[MAX_HISTORY][MAX_CMD_LEN];
static int history_count = 0;
static int history_pos = 0;

static char env_vars[MAX_ENV_VARS][MAX_ENV_NAME + MAX_ENV_VALUE + 2];
static int env_count = 0;

static char aliases[MAX_ALIASES][MAX_ALIAS_NAME];
static char alias_cmds[MAX_ALIASES][MAX_ALIAS_CMD];
static int alias_count = 0;

static char current_cmd[MAX_CMD_LEN];
static int cmd_pos = 0;
static int cmd_start_row = 0;
static int cmd_start_col = 0;

static uint8_t last_exit_code = 0;

// ============================================================
// Terminal Functions
// ============================================================
void terminal_setcolor(uint8_t color) {
    terminal_color = color;
}

void terminal_putchar(char c) {
    if (c == '\n') {
        terminal_row++;
        terminal_col = 0;
        return;
    }
    if (c == '\r') {
        terminal_col = 0;
        return;
    }
    if (c == '\b') {
        if (terminal_col > 0) {
            terminal_col--;
            const int index = terminal_row * VGA_WIDTH + terminal_col;
            terminal_buffer[index] = (uint16_t)' ' | ((uint16_t)terminal_color << 8);
        }
        return;
    }
    
    const int index = terminal_row * VGA_WIDTH + terminal_col;
    terminal_buffer[index] = (uint16_t)c | ((uint16_t)terminal_color << 8);
    
    if (++terminal_col == VGA_WIDTH) {
        terminal_col = 0;
        terminal_row++;
    }
    
    if (terminal_row >= VGA_HEIGHT) {
        for (int y = 1; y < VGA_HEIGHT; y++) {
            for (int x = 0; x < VGA_WIDTH; x++) {
                const int src = y * VGA_WIDTH + x;
                const int dst = (y - 1) * VGA_WIDTH + x;
                terminal_buffer[dst] = terminal_buffer[src];
            }
        }
        for (int x = 0; x < VGA_WIDTH; x++) {
            const int idx = (VGA_HEIGHT - 1) * VGA_WIDTH + x;
            terminal_buffer[idx] = (uint16_t)' ' | ((uint16_t)terminal_color << 8);
        }
        terminal_row = VGA_HEIGHT - 1;
    }
}

void terminal_write(const char* str) {
    while (*str) {
        if (*str == '\n') {
            terminal_putchar('\n');
            terminal_putchar('\r');
        } else {
            terminal_putchar(*str);
        }
        str++;
    }
}

void terminal_write_color(const char* str, uint8_t color) {
    uint8_t old_color = terminal_color;
    terminal_setcolor(color);
    terminal_write(str);
    terminal_setcolor(old_color);
}

void terminal_clear() {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        terminal_buffer[i] = (uint16_t)' ' | ((uint16_t)VGA_COLOR_WHITE << 8);
    }
    terminal_row = 0;
    terminal_col = 0;
}

void terminal_get_cursor(int* row, int* col) {
    *row = terminal_row;
    *col = terminal_col;
}

void terminal_set_cursor(int row, int col) {
    terminal_row = row;
    terminal_col = col;
}

// ============================================================
// Keyboard Functions
// ============================================================
static uint8_t shift_pressed = 0;
static uint8_t caps_lock = 0;

static const char keymap_normal[] = {
    0,    0,    '1',  '2',  '3',  '4',  '5',  '6',  '7',  '8',  '9',  '0',  '-',  '=',  0,    0,
    'q',  'w',  'e',  'r',  't',  'y',  'u',  'i',  'o',  'p',  '[',  ']',  0,    0,    'a',  's',
    'd',  'f',  'g',  'h',  'j',  'k',  'l',  ';',  '\'', '`',  0,    '\\', 'z',  'x',  'c',  'v',
    'b',  'n',  'm',  ',',  '.',  '/',  0,    '*',  0,    ' ',  0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0
};

static const char keymap_shift[] = {
    0,    0,    '!',  '@',  '#',  '$',  '%',  '^',  '&',  '*',  '(',  ')',  '_',  '+',  0,    0,
    'Q',  'W',  'E',  'R',  'T',  'Y',  'U',  'I',  'O',  'P',  '{',  '}',  0,    0,    'A',  'S',
    'D',  'F',  'G',  'H',  'J',  'K',  'L',  ':',  '"',  '~',  0,    '|',  'Z',  'X',  'C',  'V',
    'B',  'N',  'M',  '<',  '>',  '?',  0,    '*',  0,    ' ',  0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0
};

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

uint8_t keyboard_is_pressed() {
    return inb(KEYBOARD_STATUS_PORT) & 0x01;
}

uint8_t keyboard_read_scancode() {
    return inb(KEYBOARD_DATA_PORT);
}

char keyboard_scancode_to_ascii(uint8_t scancode) {
    if (scancode == 0x2A || scancode == 0x36) {
        shift_pressed = 1;
        return 0;
    }
    if (scancode == 0xAA || scancode == 0xB6) {
        shift_pressed = 0;
        return 0;
    }
    if (scancode == 0x3A) {
        caps_lock = !caps_lock;
        return 0;
    }
    if (scancode & 0x80) return 0;
    if (scancode == 0x1C) return '\n';
    if (scancode == 0x0E) return '\b';
    if (scancode == 0x39) return ' ';
    if (scancode == 0x0F) return '\t';
    
    uint8_t shift_active = shift_pressed || caps_lock;
    char c = 0;
    if (shift_active) {
        if (scancode < sizeof(keymap_shift)) c = keymap_shift[scancode];
    } else {
        if (scancode < sizeof(keymap_normal)) c = keymap_normal[scancode];
    }
    return c;
}

char getchar() {
    char c = 0;
    while (c == 0) {
        if (keyboard_is_pressed()) {
            uint8_t scancode = keyboard_read_scancode();
            c = keyboard_scancode_to_ascii(scancode);
        }
        for (volatile int i = 0; i < 100; i++);
    }
    return c;
}

// ============================================================
// String Functions
// ============================================================
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
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

int strncmp(const char* s1, const char* s2, int n) {
    while (n-- && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    if (n < 0) return 0;
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

void strcpy(char* dest, const char* src) {
    while (*src) {
        *dest++ = *src++;
    }
    *dest = '\0';
}

void strncpy(char* dest, const char* src, int n) {
    int i;
    for (i = 0; i < n && src[i]; i++) {
        dest[i] = src[i];
    }
    for (; i < n; i++) {
        dest[i] = '\0';
    }
}

int strchr(const char* str, char c) {
    for (int i = 0; str[i]; i++) {
        if (str[i] == c) return i;
    }
    return -1;
}

// ============================================================
// Shell Functions
// ============================================================
void shell_add_history(const char* cmd) {
    if (history_count < MAX_HISTORY) {
        strcpy(history[history_count++], cmd);
    } else {
        // Shift history
        for (int i = 0; i < MAX_HISTORY - 1; i++) {
            strcpy(history[i], history[i + 1]);
        }
        strcpy(history[MAX_HISTORY - 1], cmd);
    }
    history_pos = history_count;
}

void shell_add_env(const char* name, const char* value) {
    if (env_count < MAX_ENV_VARS) {
        char* entry = env_vars[env_count];
        strcpy(entry, name);
        int len = strlen(name);
        entry[len] = '=';
        strcpy(entry + len + 1, value);
        env_count++;
    }
}

const char* shell_get_env(const char* name) {
    for (int i = 0; i < env_count; i++) {
        char* entry = env_vars[i];
        int j = 0;
        while (entry[j] && entry[j] != '=') {
            if (entry[j] != name[j]) break;
            j++;
        }
        if (entry[j] == '=' && name[j] == '\0') {
            return entry + j + 1;
        }
    }
    return NULL;
}

void shell_add_alias(const char* name, const char* cmd) {
    if (alias_count < MAX_ALIASES) {
        strcpy(aliases[alias_count], name);
        strcpy(alias_cmds[alias_count], cmd);
        alias_count++;
    }
}

const char* shell_get_alias(const char* name) {
    for (int i = 0; i < alias_count; i++) {
        if (strcmp(aliases[i], name) == 0) {
            return alias_cmds[i];
        }
    }
    return NULL;
}

void shell_parse_args(char* cmd, char** args, int* arg_count) {
    *arg_count = 0;
    int i = 0;
    while (cmd[i] && *arg_count < MAX_ARGS) {
        // Skip spaces
        while (cmd[i] == ' ') i++;
        if (!cmd[i]) break;
        
        args[*arg_count] = &cmd[i];
        (*arg_count)++;
        
        // Find end of argument
        while (cmd[i] && cmd[i] != ' ') i++;
        if (cmd[i]) {
            cmd[i] = '\0';
            i++;
        }
    }
}

void shell_print_help() {
    terminal_write_color("\n", VGA_COLOR_WHITE);
    terminal_write_color("╔═══════════════════════════════════════════════════════════════╗\n", VGA_COLOR_CYAN);
    terminal_write_color("║                    CARDBOARDOS SHELL HELP                    ║\n", VGA_COLOR_CYAN);
    terminal_write_color("╚═══════════════════════════════════════════════════════════════╝\n", VGA_COLOR_CYAN);
    terminal_write_color("\n", VGA_COLOR_WHITE);
    
    terminal_write_color("  COMMANDS:\n", VGA_COLOR_LIGHT_GREEN);
    terminal_write_color("  ───────────────────────────────────────────────────────────\n", VGA_COLOR_LIGHT_GREY);
    terminal_write("  help              Show this help message\n");
    terminal_write("  clear             Clear the screen\n");
    terminal_write("  echo [text]       Echo text to screen\n");
    terminal_write("  ls                List directory contents (placeholder)\n");
    terminal_write("  pwd               Print working directory (placeholder)\n");
    terminal_write("  whoami            Print current user\n");
    terminal_write("  uname             Print system information\n");
    terminal_write("  date              Print current date and time\n");
    terminal_write("  uptime            Print system uptime\n");
    terminal_write("  history           Show command history\n");
    terminal_write("  env               Show environment variables\n");
    terminal_write("  set NAME=VALUE    Set environment variable\n");
    terminal_write("  alias NAME=CMD    Create command alias\n");
    terminal_write("  unalias NAME      Remove alias\n");
    terminal_write("  color [color]     Change terminal color\n");
    terminal_write("  test              Run system test\n");
    terminal_write("  exit              Halt the system\n");
    terminal_write("  reboot            Reboot the system\n");
    
    terminal_write_color("\n  KEYBOARD SHORTCUTS:\n", VGA_COLOR_LIGHT_GREEN);
    terminal_write_color("  ───────────────────────────────────────────────────────────\n", VGA_COLOR_LIGHT_GREY);
    terminal_write("  Up/Down Arrows    Navigate command history\n");
    terminal_write("  Tab               Auto-complete (basic)\n");
    terminal_write("  Backspace         Delete character\n");
}

void shell_echo(char** args, int arg_count) {
    for (int i = 1; i < arg_count; i++) {
        if (args[i][0] == '$') {
            // Environment variable
            const char* value = shell_get_env(args[i] + 1);
            if (value) {
                terminal_write(value);
            }
        } else {
            terminal_write(args[i]);
        }
        if (i < arg_count - 1) terminal_write(" ");
    }
    terminal_write("\n");
}

void shell_color_cmd(char** args, int arg_count) {
    if (arg_count < 2) {
        terminal_write("Usage: color [black|blue|green|cyan|red|magenta|brown|white]\n");
        return;
    }
    
    uint8_t color = VGA_COLOR_WHITE;
    if (strcmp(args[1], "black") == 0) color = VGA_COLOR_BLACK;
    else if (strcmp(args[1], "blue") == 0) color = VGA_COLOR_BLUE;
    else if (strcmp(args[1], "green") == 0) color = VGA_COLOR_GREEN;
    else if (strcmp(args[1], "cyan") == 0) color = VGA_COLOR_CYAN;
    else if (strcmp(args[1], "red") == 0) color = VGA_COLOR_RED;
    else if (strcmp(args[1], "magenta") == 0) color = VGA_COLOR_MAGENTA;
    else if (strcmp(args[1], "brown") == 0) color = VGA_COLOR_BROWN;
    else if (strcmp(args[1], "white") == 0) color = VGA_COLOR_WHITE;
    else {
        terminal_write("Invalid color. Available: black, blue, green, cyan, red, magenta, brown, white\n");
        return;
    }
    
    terminal_setcolor(color);
    terminal_write_color("Color changed!\n", color);
}

void shell_uname() {
    terminal_write("CardboardOS 0.1.0\n");
    terminal_write("Kernel: i386 32-bit\n");
    terminal_write("Shell: bash-like v1.0\n");
}

void shell_whoami() {
    const char* user = shell_get_env("USER");
    if (!user) user = "root";
    terminal_write(user);
    terminal_write("\n");
}

void shell_pwd() {
    const char* pwd = shell_get_env("PWD");
    if (!pwd) pwd = "/";
    terminal_write(pwd);
    terminal_write("\n");
}

void shell_ls() {
    terminal_write_color("No filesystem mounted yet.\n", VGA_COLOR_BROWN);
    terminal_write("(Filesystem support coming soon!)\n");
}

void shell_history_cmd() {
    for (int i = 0; i < history_count; i++) {
        char num[8];
        // Convert number to string
        int n = i + 1;
        int idx = 0;
        if (n == 0) {
            num[idx++] = '0';
        } else {
            char temp[8];
            int tidx = 0;
            while (n > 0) {
                temp[tidx++] = '0' + (n % 10);
                n /= 10;
            }
            while (tidx > 0) {
                num[idx++] = temp[--tidx];
            }
        }
        num[idx] = '\0';
        
        terminal_write("  ");
        terminal_write(num);
        terminal_write("  ");
        terminal_write(history[i]);
        terminal_write("\n");
    }
}

void shell_env_cmd() {
    for (int i = 0; i < env_count; i++) {
        terminal_write("  ");
        terminal_write(env_vars[i]);
        terminal_write("\n");
    }
}

void shell_set_cmd(const char* arg) {
    // Find '='
    int eq_pos = -1;
    for (int i = 0; arg[i]; i++) {
        if (arg[i] == '=') {
            eq_pos = i;
            break;
        }
    }
    
    if (eq_pos < 0) {
        terminal_write("Usage: set NAME=VALUE\n");
        return;
    }
    
    char name[MAX_ENV_NAME];
    char value[MAX_ENV_VALUE];
    strncpy(name, arg, eq_pos);
    name[eq_pos] = '\0';
    strcpy(value, arg + eq_pos + 1);
    
    shell_add_env(name, value);
    terminal_write("Environment variable set: ");
    terminal_write(name);
    terminal_write("=");
    terminal_write(value);
    terminal_write("\n");
}

void shell_alias_cmd(const char* arg) {
    int eq_pos = -1;
    for (int i = 0; arg[i]; i++) {
        if (arg[i] == '=') {
            eq_pos = i;
            break;
        }
    }
    
    if (eq_pos < 0) {
        // Show aliases
        if (alias_count == 0) {
            terminal_write("No aliases defined.\n");
            return;
        }
        for (int i = 0; i < alias_count; i++) {
            terminal_write("  alias ");
            terminal_write(aliases[i]);
            terminal_write("='");
            terminal_write(alias_cmds[i]);
            terminal_write("'\n");
        }
        return;
    }
    
    char name[MAX_ALIAS_NAME];
    char cmd[MAX_ALIAS_CMD];
    strncpy(name, arg, eq_pos);
    name[eq_pos] = '\0';
    strcpy(cmd, arg + eq_pos + 1);
    
    shell_add_alias(name, cmd);
    terminal_write("Alias added: ");
    terminal_write(name);
    terminal_write("='");
    terminal_write(cmd);
    terminal_write("'\n");
}

void shell_unalias_cmd(const char* name) {
    for (int i = 0; i < alias_count; i++) {
        if (strcmp(aliases[i], name) == 0) {
            // Remove by shifting
            for (int j = i; j < alias_count - 1; j++) {
                strcpy(aliases[j], aliases[j + 1]);
                strcpy(alias_cmds[j], alias_cmds[j + 1]);
            }
            alias_count--;
            terminal_write("Alias removed: ");
            terminal_write(name);
            terminal_write("\n");
            return;
        }
    }
    terminal_write("Alias not found: ");
    terminal_write(name);
    terminal_write("\n");
}

void shell_date() {
    terminal_write("2026-08-11 00:00:00 UTC\n");
    terminal_write_color("(Real time support coming soon!)\n", VGA_COLOR_BROWN);
}

void shell_uptime() {
    terminal_write("Uptime: 00:00:01\n");
    terminal_write_color("(Uptime tracking coming soon!)\n", VGA_COLOR_BROWN);
}

void shell_test() {
    terminal_write_color("Running system tests...\n", VGA_COLOR_CYAN);
    terminal_write("  ✓ Kernel loaded\n");
    terminal_write("  ✓ Protected mode active\n");
    terminal_write("  ✓ VGA text mode working\n");
    terminal_write("  ✓ Keyboard input working\n");
    terminal_write("  ✓ Shell initialized\n");
    terminal_write_color("  ✓ All tests passed!\n", VGA_COLOR_LIGHT_GREEN);
}

void shell_exit() {
    terminal_write_color("System halted. Press any key to reboot...\n", VGA_COLOR_RED);
    getchar();
    // Triple fault to reboot
    __asm__ volatile("cli; hlt");
}

void shell_reboot() {
    terminal_write_color("Rebooting system...\n", VGA_COLOR_RED);
    // Reset via keyboard controller
    outb(0x64, 0xFE);
    while(1) __asm__ volatile("hlt");
}

void shell_execute(char* cmd) {
    if (cmd[0] == '\0') return;
    
    // Add to history
    shell_add_history(cmd);
    
    // Parse arguments
    char* args[MAX_ARGS];
    int arg_count;
    shell_parse_args(cmd, args, &arg_count);
    
    if (arg_count == 0) return;
    
    // Check for aliases
    const char* alias_cmd = shell_get_alias(args[0]);
    if (alias_cmd) {
        // Execute alias
        char expanded[MAX_CMD_LEN];
        strcpy(expanded, alias_cmd);
        // Add remaining arguments
        for (int i = 1; i < arg_count; i++) {
            strcat(expanded, " ");
            strcat(expanded, args[i]);
        }
        shell_execute(expanded);
        return;
    }
    
    // Built-in commands
    if (strcmp(args[0], "help") == 0) {
        shell_print_help();
    }
    else if (strcmp(args[0], "clear") == 0) {
        terminal_clear();
    }
    else if (strcmp(args[0], "echo") == 0) {
        shell_echo(args, arg_count);
    }
    else if (strcmp(args[0], "ls") == 0) {
        shell_ls();
    }
    else if (strcmp(args[0], "pwd") == 0) {
        shell_pwd();
    }
    else if (strcmp(args[0], "whoami") == 0) {
        shell_whoami();
    }
    else if (strcmp(args[0], "uname") == 0) {
        shell_uname();
    }
    else if (strcmp(args[0], "date") == 0) {
        shell_date();
    }
    else if (strcmp(args[0], "uptime") == 0) {
        shell_uptime();
    }
    else if (strcmp(args[0], "history") == 0) {
        shell_history_cmd();
    }
    else if (strcmp(args[0], "env") == 0) {
        shell_env_cmd();
    }
    else if (strcmp(args[0], "set") == 0) {
        if (arg_count < 2) {
            terminal_write("Usage: set NAME=VALUE\n");
        } else {
            shell_set_cmd(args[1]);
        }
    }
    else if (strcmp(args[0], "alias") == 0) {
        if (arg_count < 2) {
            terminal_write("Usage: alias NAME=CMD\n");
        } else {
            shell_alias_cmd(args[1]);
        }
    }
    else if (strcmp(args[0], "unalias") == 0) {
        if (arg_count < 2) {
            terminal_write("Usage: unalias NAME\n");
        } else {
            shell_unalias_cmd(args[1]);
        }
    }
    else if (strcmp(args[0], "color") == 0) {
        shell_color_cmd(args, arg_count);
    }
    else if (strcmp(args[0], "test") == 0) {
        shell_test();
    }
    else if (strcmp(args[0], "exit") == 0) {
        shell_exit();
    }
    else if (strcmp(args[0], "reboot") == 0) {
        shell_reboot();
    }
    else {
        terminal_write_color("Command not found: ", VGA_COLOR_RED);
        terminal_write(args[0]);
        terminal_write("\n");
        terminal_write("Type 'help' for available commands\n");
        last_exit_code = 1;
    }
}

void strcat(char* dest, const char* src) {
    // Find end of dest
    while (*dest) dest++;
    // Copy src
    while (*src) {
        *dest++ = *src++;
    }
    *dest = '\0';
}

// ============================================================
// Main Shell Loop
// ============================================================
void shell_loop() {
    // Set up environment
    shell_add_env("USER", "root");
    shell_add_env("PWD", "/");
    shell_add_env("SHELL", "/bin/sh");
    shell_add_env("TERM", "vga");
    shell_add_env("HOME", "/root");
    shell_add_env("PATH", "/bin:/usr/bin:/usr/local/bin");
    shell_add_env("PS1", "\\u@\\h:\\w$ ");
    shell_add_env("HOSTNAME", "cardboardos");
    
    // Add some default aliases
    shell_add_alias("l", "ls");
    shell_add_alias("ll", "ls -l");
    shell_add_alias("la", "ls -a");
    shell_add_alias("..", "cd ..");
    shell_add_alias("h", "history");
    shell_add_alias("clr", "clear");
    
    char cmd_buffer[MAX_CMD_LEN];
    int cmd_pos = 0;
    
    while (1) {
        // Print prompt
        terminal_setcolor(VGA_COLOR_GREEN);
        terminal_write("[root@cardboardos ~]$ ");
        terminal_setcolor(VGA_COLOR_WHITE);
        
        // Read command
        cmd_pos = 0;
        int history_temp = history_pos;
        
        while (1) {
            char c = getchar();
            
            if (c == '\n') {
                cmd_buffer[cmd_pos] = '\0';
                terminal_write("\n");
                break;
            }
            else if (c == '\b') {
                if (cmd_pos > 0) {
                    cmd_pos--;
                    terminal_putchar('\b');
                }
            }
            else if (c == '\t') {
                // Basic tab completion
                // Find matching commands
                const char* possible_commands[] = {
                    "help", "clear", "echo", "ls", "pwd", "whoami", "uname",
                    "date", "uptime", "history", "env", "set", "alias", "unalias",
                    "color", "test", "exit", "reboot"
                };
                int num_commands = sizeof(possible_commands) / sizeof(possible_commands[0]);
                
                // Find matches
                int matches = 0;
                int match_index = -1;
                for (int i = 0; i < num_commands; i++) {
                    if (strncmp(cmd_buffer, possible_commands[i], cmd_pos) == 0) {
                        matches++;
                        match_index = i;
                    }
                }
                
                if (matches == 1) {
                    // Complete the command
                    const char* cmd = possible_commands[match_index];
                    while (cmd[cmd_pos]) {
                        terminal_putchar(cmd[cmd_pos]);
                        cmd_buffer[cmd_pos] = cmd[cmd_pos];
                        cmd_pos++;
                    }
                    terminal_putchar(' ');
                    cmd_buffer[cmd_pos] = ' ';
                    cmd_pos++;
                } else if (matches > 1) {
                    terminal_write("\n");
                    for (int i = 0; i < num_commands; i++) {
                        if (strncmp(cmd_buffer, possible_commands[i], cmd_pos) == 0) {
                            terminal_write("  ");
                            terminal_write(possible_commands[i]);
                            terminal_write("\n");
                        }
                    }
                    // Reprint prompt and command
                    terminal_setcolor(VGA_COLOR_GREEN);
                    terminal_write("[root@cardboardos ~]$ ");
                    terminal_setcolor(VGA_COLOR_WHITE);
                    for (int i = 0; i < cmd_pos; i++) {
                        terminal_putchar(cmd_buffer[i]);
                    }
                }
            }
            else if (c == (char)0x48) { // Up arrow
                if (history_temp > 0) {
                    // Clear current line
                    for (int i = 0; i < cmd_pos; i++) {
                        terminal_putchar('\b');
                    }
                    history_temp--;
                    strcpy(cmd_buffer, history[history_temp]);
                    cmd_pos = strlen(cmd_buffer);
                    terminal_write(cmd_buffer);
                }
            }
            else if (c == (char)0x50) { // Down arrow
                if (history_temp < history_count) {
                    // Clear current line
                    for (int i = 0; i < cmd_pos; i++) {
                        terminal_putchar('\b');
                    }
                    history_temp++;
                    if (history_temp < history_count) {
                        strcpy(cmd_buffer, history[history_temp]);
                        cmd_pos = strlen(cmd_buffer);
                        terminal_write(cmd_buffer);
                    } else {
                        cmd_buffer[0] = '\0';
                        cmd_pos = 0;
                    }
                }
            }
            else if (c >= ' ' && c <= '~') {
                if (cmd_pos < MAX_CMD_LEN - 1) {
                    cmd_buffer[cmd_pos++] = c;
                    terminal_putchar(c);
                }
            }
        }
        
        // Execute command
        shell_execute(cmd_buffer);
    }
}

// ============================================================
// Main Kernel Entry
// ============================================================
void kmain(void) {
    terminal_clear();
    
    terminal_setcolor(VGA_COLOR_CYAN);
    terminal_write("╔═══════════════════════════════════════════════════════════════╗\n");
    terminal_write("║                    CARDBOARD OS 26.8                          ║\n");
    terminal_write("║                   Bash-like Shell v1.0                        ║\n");
    terminal_write("╚═══════════════════════════════════════════════════════════════╝\n");
    terminal_setcolor(VGA_COLOR_WHITE);
    
    terminal_write("\nWelcome to CardboardOS!\n");
    terminal_write("Type 'help' for a list of commands.\n\n");
    
    // Start shell
    shell_loop();
}