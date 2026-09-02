/**
 * CardboardOS - Standard Library
 * Freestanding stdlib functions
 */

#include <stddef.h>
#include <stdint.h>

// Simple random number generator
static uint32_t seed = 0x12345678;

uint32_t rand(void) {
    seed = seed * 1103515245 + 12345;
    return (uint32_t)(seed / 65536) % 32768;
}

void srand(uint32_t new_seed) {
    seed = new_seed;
}

// Simple itoa implementation
char* itoa(int value, char* str, int base) {
    char* rc = str;
    char* ptr = str;
    char* low = str;
    uint32_t num;
    
    // Negative only works for base 10
    if (base < 2 || base > 36) {
        *str = '\0';
        return str;
    }
    
    if (value < 0 && base == 10) {
        *ptr++ = '-';
        low++;
        num = -value;
    } else {
        num = value;
    }
    
    // Convert to string (backwards)
    do {
        int digit = num % base;
        *ptr++ = (digit < 10) ? '0' + digit : 'a' + digit - 10;
        num /= base;
    } while (num);
    
    *ptr-- = '\0';
    
    // Reverse the string
    while (low < ptr) {
        char tmp = *low;
        *low++ = *ptr;
        *ptr-- = tmp;
    }
    
    return rc;
}

// Simple atoi
int atoi(const char* str) {
    int result = 0;
    int sign = 1;
    
    while (*str == ' ' || *str == '\t') str++;
    
    if (*str == '-') {
        sign = -1;
        str++;
    } else if (*str == '+') {
        str++;
    }
    
    while (*str >= '0' && *str <= '9') {
        result = result * 10 + (*str - '0');
        str++;
    }
    
    return result * sign;
}