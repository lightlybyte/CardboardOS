/**
 * CardboardOS - Public Kernel Header
 * This is the main public header for kernel functions
 */

#ifndef KERNEL_H
#define KERNEL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Version information
#define CARD_BOARD_OS_VERSION "0.1.0"
#define CARD_BOARD_OS_BUILD_DATE __DATE__
#define CARD_BOARD_OS_BUILD_TIME __TIME__

// System types
typedef uint8_t  byte;
typedef uint16_t word;
typedef uint32_t dword;
typedef uint64_t qword;

// VGA functions
void terminal_initialize(void);
void terminal_setcolor(uint8_t color);
void terminal_putchar(char c);
void terminal_writestring(const char* data);
void terminal_write(const char* data, size_t size);

// Memory functions
void* malloc(size_t size);
void free(void* ptr);
void* calloc(size_t num, size_t size);
void* realloc(void* ptr, size_t new_size);

// String functions
size_t strlen(const char* str);
char* strcpy(char* dest, const char* src);
int strcmp(const char* s1, const char* s2);
void* memset(void* ptr, int value, size_t num);
void* memcpy(void* dest, const void* src, size_t num);

// System functions
void panic(const char* message);
void reboot(void);
void shutdown(void);

// NotC interpreter
void notc_interpret(const char* source);
void notc_run_file(const char* filename);

// GUI functions
void gui_init(void);
void gui_update(void);
void gui_create_window(const char* title, int x, int y, int width, int height);

#endif // KERNEL_H