/**
 * CardboardOS - Kernel Panic Header
 */

#ifndef PANIC_H
#define PANIC_H

void panic(const char* message);
void assert_fail(const char* file, int line, const char* function, const char* expression);

// Assert macro
#define assert(expr) \
    do { \
        if (!(expr)) { \
            assert_fail(__FILE__, __LINE__, __func__, #expr); \
        } \
    } while(0)

#endif // PANIC_H