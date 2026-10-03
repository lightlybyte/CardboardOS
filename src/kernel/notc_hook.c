#include <stdint.h>

extern void kprint(const char *s);

void notc_write_hook(const char *s) {
    kprint(s);
    kprint("\n");
}