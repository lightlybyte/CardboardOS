#ifndef _STDIO_H
#define _STDIO_H

#include <stddef.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _FILE FILE;

int printf(const char *fmt, ...);
int sprintf(char *out, const char *fmt, ...);
int snprintf(char *out, size_t n, const char *fmt, ...);
int vsnprintf(char *out, size_t n, const char *fmt, va_list ap);
int puts(const char *s);
int putchar(int c);

#ifdef __cplusplus
}
#endif

#endif