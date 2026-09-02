/**
 * CardboardOS - Timer/PIT Header
 */

#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

// Timer callback function type
typedef void (*timer_callback_t)(void);

// Initialize the timer (PIT)
void init_timer(uint32_t frequency);

// Register a callback for timer ticks
void register_timer_callback(timer_callback_t callback);

// Get current tick count
uint64_t get_ticks(void);

// Sleep for X milliseconds
void sleep_ms(uint32_t ms);

// Sleep for X microseconds
void sleep_us(uint32_t us);

#endif // TIMER_H