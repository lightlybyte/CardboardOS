#include <stddef.h>

extern "C" void __cxa_pure_virtual() {
    for (;;) { }
}

extern "C" void __cxa_atexit(void (*)(void*), void*, void*) { }

void* operator new(size_t)   { for (;;) { } }
void* operator new[](size_t) { for (;;) { } }
void  operator delete(void*)   { }
void  operator delete[](void*) { }
void  operator delete(void*, size_t)   { }
void  operator delete[](void*, size_t) { }