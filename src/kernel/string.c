#include <stddef.h>
#include <stdarg.h>
#include <string.h>

void *memcpy(void *dst, const void *src, size_t n) {
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    while (n--) *d++ = *s++;
    return dst;
}

void *memset(void *dst, int c, size_t n) {
    unsigned char *d = (unsigned char *)dst;
    while (n--) *d++ = (unsigned char)c;
    return dst;
}

int memcmp(const void *a, const void *b, size_t n) {
    const unsigned char *x = (const unsigned char *)a;
    const unsigned char *y = (const unsigned char *)b;
    while (n--) {
        if (*x != *y) return (int)*x - (int)*y;
        x++; y++;
    }
    return 0;
}

int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

int strncmp(const char *a, const char *b, size_t n) {
    while (n && *a && *a == *b) { a++; b++; n--; }
    if (n == 0) return 0;
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

char *strcpy(char *dst, const char *src) {
    char *d = dst;
    while ((*d++ = *src++)) { }
    return dst;
}

char *strncpy(char *dst, const char *src, size_t n) {
    char *d = dst;
    while (n && (*d++ = *src++)) n--;
    while (n--) *d++ = 0;
    return dst;
}

char *strncat(char *dst, const char *src, size_t n) {
    char *d = dst + strlen(dst);
    while (n-- && (*d++ = *src++)) { }
    *d = 0;
    return dst;
}

char *strchr(const char *s, int c) {
    while (*s) {
        if (*s == (char)c) return (char *)s;
        s++;
    }
    if ((char)c == 0) return (char *)s;
    return 0;
}

size_t strlen(const char *s) {
    const char *p = s;
    while (*p) p++;
    return (size_t)(p - s);
}

static void reverse_str(char *s, int len) {
    for (int i = 0, j = len - 1; i < j; i++, j--) {
        char t = s[i]; s[i] = s[j]; s[j] = t;
    }
}

static int itoa_ll(long long v, char *buf) {
    int i = 0;
    int neg = v < 0;
    unsigned long long u = neg ? -(unsigned long long)v : (unsigned long long)v;
    if (u == 0) { buf[i++] = '0'; }
    while (u) { buf[i++] = (char)('0' + (u % 10)); u /= 10; }
    if (neg) buf[i++] = '-';
    reverse_str(buf, i);
    return i;
}

int vsnprintf(char *out, size_t n, const char *fmt, va_list ap) {
    size_t pos = 0;
    if (n == 0) return 0;

    while (*fmt) {
        if (pos + 1 >= n) break;
        if (*fmt != '%') { out[pos++] = *fmt++; continue; }
        fmt++;

        if (*fmt == 's') {
            const char *s = va_arg(ap, const char *);
            if (!s) s = "(null)";
            while (*s && pos + 1 < n) out[pos++] = *s++;
            fmt++;
        } else if (*fmt == 'd' || *fmt == 'i') {
            char tmp[32];
            int len = itoa_ll(va_arg(ap, int), tmp);
            for (int i = 0; i < len && pos + 1 < n; i++) out[pos++] = tmp[i];
            fmt++;
        } else if (*fmt == 'l' && *(fmt + 1) == 'l' && *(fmt + 2) == 'd') {
            char tmp[32];
            int len = itoa_ll(va_arg(ap, long long), tmp);
            for (int i = 0; i < len && pos + 1 < n; i++) out[pos++] = tmp[i];
            fmt += 3;
        } else if (*fmt == 'u') {
            char tmp[32];
            int len = itoa_ll((long long)va_arg(ap, unsigned int), tmp);
            for (int i = 0; i < len && pos + 1 < n; i++) out[pos++] = tmp[i];
            fmt++;
        } else if (*fmt == 'x') {
            unsigned int v = va_arg(ap, unsigned int);
            char tmp[16]; int len = 0;
            if (v == 0) tmp[len++] = '0';
            while (v) {
                int d = v & 0xF;
                tmp[len++] = (char)(d < 10 ? '0' + d : 'a' + d - 10);
                v >>= 4;
            }
            reverse_str(tmp, len);
            for (int i = 0; i < len && pos + 1 < n; i++) out[pos++] = tmp[i];
            fmt++;
        } else if (*fmt == 'c') {
            out[pos++] = (char)va_arg(ap, int);
            fmt++;
        } else if (*fmt == '%') {
            out[pos++] = '%';
            fmt++;
        } else {
            out[pos++] = '%';
        }
    }
    out[pos] = 0;
    return (int)pos;
}

int snprintf(char *out, size_t n, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vsnprintf(out, n, fmt, ap);
    va_end(ap);
    return r;
}