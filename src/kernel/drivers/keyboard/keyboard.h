/**
 * CardboardOS - Keyboard Driver Header
 */

#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>
#include <stdbool.h>

// Keyboard callback function type
typedef void (*keyboard_callback_t)(char key);

// Initialize keyboard driver
void init_keyboard(void);

// Register keyboard callback
void register_keyboard_callback(keyboard_callback_t callback);

// Check if a key is pressed
bool is_key_pressed(uint8_t scancode);

// Get last pressed key
char get_last_key(void);

// Wait for a key press
char wait_for_key(void);

#endif // KEYBOARD_H