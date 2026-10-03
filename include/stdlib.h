#ifndef _STDLIB_H
#define _STDLIB_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void *malloc(size_t n);
void  free(void *p);
void *calloc(size_t n, size_t sz);
void *realloc(void *p, size_t n);
int   atoi(const char *s);
int   abs(int x);
void  exit(int code);
void  abort(void);

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

#ifdef __cplusplus
}
#endif

#endif