/**
 * CardboardOS - Timer/PIT Implementation
 */

#include "timer.h"
#include "interrupts.h"
#include "../core/panic.h"
#include "../core/kmain.h"

// PIT ports
#define PIT_PORT_CH0 0x40
#define PIT_PORT_CH1 0x41
#define PIT_PORT_CH2 0x42
#define PIT_PORT_CMD 0x43

// PIT command flags
#define PIT_CMD_CH0 0x00
#define PIT_CMD_CH1 0x40
#define PIT_CMD_CH2 0x80
#define PIT_CMD_READBACK 0xC0
#define PIT_CMD_ACCESS_LOW 0x10
#define PIT_CMD_ACCESS_HIGH 0x20
#define PIT_CMD_ACCESS_WORD 0x30
#define PIT_CMD_MODE_TERMINAL 0x00
#define PIT_CMD_MODE_ONESHOT 0x02
#define PIT_CMD_MODE_RATE 0x04
#define PIT_CMD_MODE_SQUARE 0x06
#define PIT_CMD_MODE_STROBE 0x08
#define PIT_CMD_MODE_HARDWARE 0x0A
#define PIT_CMD_MODE_RATE_GATE 0x0C
#define PIT_CMD_MODE_SQUARE_GATE 0x0E
#define PIT_CMD_BINARY 0x00
#define PIT_CMD_BCD 0x01

// PIT frequency
#define PIT_BASE_FREQUENCY 1193182

static volatile uint64_t ticks = 0;
static timer_callback_t callback = NULL;

// Timer interrupt handler
static void timer_handler(void) {
    ticks++;
    if (callback) {
        callback();
    }
}

void init_timer(uint32_t frequency) {
    // Calculate divisor
    uint32_t divisor = PIT_BASE_FREQUENCY / frequency;
    
    // Send command byte
    outb(PIT_PORT_CMD, PIT_CMD_CH0 | PIT_CMD_ACCESS_WORD | PIT_CMD_MODE_RATE | PIT_CMD_BINARY);
    
    // Send divisor low byte
    outb(PIT_PORT_CH0, divisor & 0xFF);
    
    // Send divisor high byte
    outb(PIT_PORT_CH0, (divisor >> 8) & 0xFF);
    
    // Register timer interrupt handler
    register_interrupt_handler(0, timer_handler);
}

void register_timer_callback(timer_callback_t cb) {
    callback = cb;
}

uint64_t get_ticks(void) {
    return ticks;
}

void sleep_ms(uint32_t ms) {
    uint64_t target = ticks + (ms * 1000 / 55); // Approximate
    while (ticks < target) {
        __asm__ volatile("hlt");
    }
}

void sleep_us(uint32_t us) {
    // Simple busy wait for microseconds
    // Not accurate, but works for small delays
    for (uint32_t i = 0; i < us * 100; i++) {
        __asm__ volatile("pause");
    }
}

// Helper functions for port I/O
static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}