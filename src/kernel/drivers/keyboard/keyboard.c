/**
 * CardboardOS - Keyboard Driver Implementation
 */

#include "keyboard.h"
#include "../../sys/interrupts.h"
#include "../../sys/timer.h"
#include "../../core/panic.h"

// Keyboard ports
#define KEYBOARD_PORT_DATA 0x60
#define KEYBOARD_PORT_CMD 0x64
#define KEYBOARD_PORT_STATUS 0x64

// Keyboard commands
#define KEYBOARD_CMD_SET_LEDS 0xED
#define KEYBOARD_CMD_ECHO 0xEE
#define KEYBOARD_CMD_SET_SCANCODE 0xF0
#define KEYBOARD_CMD_SET_TYPEMATIC 0xF3
#define KEYBOARD_CMD_ENABLE 0xF4
#define KEYBOARD_CMD_DISABLE 0xF5
#define KEYBOARD_CMD_SET_DEFAULTS 0xF6
#define KEYBOARD_CMD_RESEND 0xFE
#define KEYBOARD_CMD_RESET 0xFF

// Key states
#define KEY_RELEASED 0x80
#define KEY_PRESSED 0x00

// US QWERTY scancode to ASCII mapping
static const char scancode_to_ascii[] = {
    0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 0,
    0, 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', 0,
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,
    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0,
    ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '-', 0, 0, 0, '+'
};

// Shift key mapping
static const char scancode_to_ascii_shift[] = {
    0, 0, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', 0,
    0, 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', 0,
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0,
    '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*', 0,
    ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '-', 0, 0, 0, '+'
};

// Key states
static bool key_states[256] = {0};
static char last_key = 0;
static bool shift_pressed = false;
static bool caps_lock = false;
static bool ctrl_pressed = false;
static bool alt_pressed = false;

static keyboard_callback_t callback = NULL;

// Read keyboard status
static uint8_t read_status(void) {
    uint8_t status;
    __asm__ volatile("inb $0x64, %0" : "=a"(status));
    return status;
}

// Read keyboard data
static uint8_t read_data(void) {
    uint8_t data;
    __asm__ volatile("inb $0x60, %0" : "=a"(data));
    return data;
}

// Send command to keyboard
static void send_command(uint8_t cmd) {
    // Wait for keyboard to be ready
    while (read_status() & 0x02) {
        __asm__ volatile("pause");
    }
    __asm__ volatile("outb %0, $0x60" : : "a"(cmd));
}

// Keyboard interrupt handler
static void keyboard_handler(void) {
    uint8_t scancode = read_data();
    bool released = scancode & KEY_RELEASED;
    uint8_t key = scancode & 0x7F;
    
    // Update key state
    key_states[key] = !released;
    
    // Handle special keys
    if (key == 0x2A || key == 0x36) { // Left/Right Shift
        shift_pressed = !released;
        return;
    } else if (key == 0x1D || key == 0x9D) { // Left/Right Ctrl
        ctrl_pressed = !released;
        return;
    } else if (key == 0x38 || key == 0xB8) { // Left/Right Alt
        alt_pressed = !released;
        return;
    } else if (key == 0x3A && !released) { // Caps Lock
        caps_lock = !caps_lock;
        return;
    }
    
    // Only handle key presses
    if (released) {
        return;
    }
    
    // Convert scancode to ASCII
    char ascii = 0;
    bool shift = shift_pressed || caps_lock;
    if (key < sizeof(scancode_to_ascii)) {
        if (shift) {
            ascii = scancode_to_ascii_shift[key];
        } else {
            ascii = scancode_to_ascii[key];
        }
    }
    
    last_key = ascii;
    
    // Call callback if registered
    if (callback && ascii) {
        callback(ascii);
    }
}

void init_keyboard(void) {
    // Reset keyboard
    send_command(KEYBOARD_CMD_RESET);
    
    // Wait for keyboard to reset
    sleep_ms(100);
    
    // Enable keyboard
    send_command(KEYBOARD_CMD_ENABLE);
    
    // Register interrupt handler
    register_interrupt_handler(1, keyboard_handler);
    
    // Clear key states
    for (int i = 0; i < 256; i++) {
        key_states[i] = false;
    }
}

void register_keyboard_callback(keyboard_callback_t cb) {
    callback = cb;
}

bool is_key_pressed(uint8_t scancode) {
    if (scancode >= 256) return false;
    return key_states[scancode];
}

char get_last_key(void) {
    char key = last_key;
    last_key = 0;
    return key;
}

char wait_for_key(void) {
    while (1) {
        char key = get_last_key();
        if (key) {
            return key;
        }
        __asm__ volatile("hlt");
    }
}