/**
 * CardboardOS - Kernel Panic Handler
 */

#include "kmain.h"

// External VGA functions
extern void vga_write(const char* str, int row, int col);
extern void vga_clear(void);

void panic(const char* message) {
    // Clear screen
    vga_clear();
    
    // Display panic message
    vga_write("╔════════════════════════════════════════╗", 0, 0);
    vga_write("║          KERNEL PANIC                  ║", 1, 0);
    vga_write("╚════════════════════════════════════════╝", 2, 0);
    vga_write("", 3, 0);
    vga_write("Error:", 4, 0);
    vga_write(message, 5, 0);
    vga_write("", 6, 0);
    vga_write("System halted. Press reset to reboot.", 7, 0);
    
    // Halt forever
    while(1) {
        __asm__ volatile("hlt");
    }
}

void assert_fail(const char* file, int line, const char* function, const char* expression) {
    vga_clear();
    vga_write("╔════════════════════════════════════════╗", 0, 0);
    vga_write("║          ASSERTION FAILED              ║", 1, 0);
    vga_write("╚════════════════════════════════════════╝", 2, 0);
    vga_write("", 3, 0);
    vga_write("File:", 4, 0);
    vga_write(file, 5, 0);
    vga_write("Line:", 6, 0);
    char line_str[16];
    // Simple conversion (will implement properly later)
    vga_write("", 7, 0);
    vga_write("Function:", 8, 0);
    vga_write(function, 9, 0);
    vga_write("Expression:", 10, 0);
    vga_write(expression, 11, 0);
    vga_write("", 12, 0);
    vga_write("System halted.", 13, 0);
    
    while(1) {
        __asm__ volatile("hlt");
    }
}