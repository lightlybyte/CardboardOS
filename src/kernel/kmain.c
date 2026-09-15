#include <stdint.h>

void kmain(uint32_t magic, uint32_t mbi_addr) {
    if (magic != 0x2BADB002) {
        // GRUB didn't boot us correctly
        for (;;) __asm__ volatile("hlt");
    }
    // serial_init(); etc.
    for (;;) __asm__ volatile("hlt");
}