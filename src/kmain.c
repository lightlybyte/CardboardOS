// src/kmain.c - Simple kernel in C
#define VIDEO_MEMORY 0xB8000
#define VGA_WIDTH 80
#define VGA_HEIGHT 25

void tinit(void);
void tputchar(char c);
void terminal_write(const char* data);
void twrite(const char* data);

// Terminal state
static int terminal_row;
static int terminal_column;
static unsigned char terminal_color;
static unsigned short* terminal_buffer;

void tinit(void) {
    terminal_row = 0;
    terminal_column = 0;
    terminal_color = 0x0F;  // White on black
    terminal_buffer = (unsigned short*) VIDEO_MEMORY;
    
    // Clear screen
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        terminal_buffer[i] = (unsigned short) (' ' | (terminal_color << 8));
    }
}

void tputchar(char c) {
    if (c == '\n') {
        terminal_row++;
        terminal_column = 0;
        return;
    }
    
    unsigned short* where = terminal_buffer + (terminal_row * VGA_WIDTH + terminal_column);
    *where = (unsigned short) (c | (terminal_color << 8));
    
    terminal_column++;
    if (terminal_column >= VGA_WIDTH) {
        terminal_column = 0;
        terminal_row++;
    }
    
    if (terminal_row >= VGA_HEIGHT) {
        // Scroll up
        for (int i = 0; i < (VGA_HEIGHT - 1) * VGA_WIDTH; i++) {
            terminal_buffer[i] = terminal_buffer[i + VGA_WIDTH];
        }
        for (int i = (VGA_HEIGHT - 1) * VGA_WIDTH; i < VGA_HEIGHT * VGA_WIDTH; i++) {
            terminal_buffer[i] = (unsigned short) (' ' | (terminal_color << 8));
        }
        terminal_row = VGA_HEIGHT - 1;
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

// Main kernel entry point
void kmain(void) {
    tinit();
    twrite("Hello from C kernel!\n");
    twrite("Bootloader loaded kmain successfully!\n");
    twrite("Running with GRUB 0.95\n");
    
    // Print some system info
    twrite("\nSystem is running...\n");
    
    // Hang forever
    while(1) {
        __asm__ volatile ("hlt");
    }
}