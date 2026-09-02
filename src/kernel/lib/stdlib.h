/**
 * CardboardOS - Standard Library Header
 */

#ifndef STDLIB_H
#define STDLIB_H

#include <stddef.h>
#include <stdint.h>

uint32_t rand(void);
void srand(uint32_t seed);
char* itoa(int value, char* str, int base);
int atoi(const char* str);

#endif // STDLIB_H