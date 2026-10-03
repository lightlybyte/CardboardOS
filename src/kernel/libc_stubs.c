#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* The kernel's text output, defined in kmain.c */
extern void kprint(const char *s);

/* The kernel's formatted output, defined in string.c */
extern int vsnprintf(char *out, size_t n, const char *fmt, va_list ap);

/* ---- stdio ---- */

int printf(const char *fmt, ...) {
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    int r = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    kprint(buf);
    return r;
}

int sprintf(char *out, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vsnprintf(out, 4096, fmt, ap);
    va_end(ap);
    return r;
}

int puts(const char *s) {
    kprint(s);
    kprint("\n");
    return 0;
}

int putchar(int c) {
    char b[2] = { (char)c, 0 };
    kprint(b);
    return c;
}

/* ---- stdlib ---- */

/* Bump allocator over a static pool. Never reclaims memory. Fine for
   GuiLite's one-time allocations; not a general-purpose heap. */
static uint8_t heap_pool[256 * 1024];
static size_t  heap_used = 0;

void *malloc(size_t n) {
    n = (n + 7) & ~(size_t)7;
    if (heap_used + n > sizeof(heap_pool)) return 0;
    void *p = heap_pool + heap_used;
    heap_used += n;
    return p;
}

void free(void *p) { (void)p; }

void *calloc(size_t n, size_t sz) {
    size_t total = n * sz;
    void *p = malloc(total);
    if (p) memset(p, 0, total);
    return p;
}

void *realloc(void *p, size_t n) {
    void *q = malloc(n);
    if (p && q) memcpy(q, p, n);
    return q;
}

int atoi(const char *s) {
    int v = 0, neg = 0;
    if (*s == '-') { neg = 1; s++; }
    while (*s >= '0' && *s <= '9') v = v * 10 + (*s++ - '0');
    return neg ? -v : v;
}

void exit(int code) {
    (void)code;
    for (;;) __asm__ volatile("hlt");
}

void abort(void) {
    for (;;) __asm__ volatile("hlt");
}

/* ---- math ---- */

double sin(double x) { (void)x; return 0.0; }
double cos(double x) { (void)x; return 1.0; }
double sqrt(double x) { return x > 0 ? x : 0; }
double fabs(double x) { return x < 0 ? -x : x; }
int abs(int x) { return x < 0 ? -x : x; }