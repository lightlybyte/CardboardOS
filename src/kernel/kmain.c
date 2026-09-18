#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include "ff.h"
#include "diskio.h"
#include "limine.h"

extern struct limine_framebuffer *get_framebuffer(void);
extern uint64_t get_hhdm_offset(void);

#define FONT_W 8
#define FONT_H 16
#define FONT_ROM_PHYS 0xFFA6E

/* ---- Port I/O ---- */
static inline uint8_t inb(uint16_t port) {
    uint8_t v;
    __asm__ volatile("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}
static inline void outb(uint16_t port, uint8_t v) {
    __asm__ volatile("outb %0, %1" : : "a"(v), "Nd"(port));
}

/* ---- Serial console (COM1) ---- */
static void serial_init(void) {
    outb(0x3F9, 0x00);
    outb(0x3FB, 0x80);
    outb(0x3F8, 0x01);
    outb(0x3F9, 0x00);
    outb(0x3FB, 0x03);
    outb(0x3FA, 0xC7);
    outb(0x3FC, 0x0B);
}
static void serial_putc(char c)          { outb(0x3F8, (uint8_t)c); }
static void serial_puts(const char *s)   { while (*s) serial_putc(*s++); }
static void serial_hex(uint64_t v) {
    const char *hex = "0123456789abcdef";
    serial_puts("0x");
    for (int i = 60; i >= 0; i -= 4) serial_putc(hex[(v >> i) & 0xF]);
}
static void serial_u64(uint64_t v) {
    char buf[21];
    int i = 0;
    if (v == 0) { serial_putc('0'); return; }
    while (v && i < 20) { buf[i++] = '0' + (v % 10); v /= 10; }
    while (i--) serial_putc(buf[i]);
}

/* ---- Framebuffer ---- */
static uint32_t *fb        = 0;
static uint32_t  fb_width  = 0;
static uint32_t  fb_height = 0;
static uint32_t  fb_pitch  = 0;
static uint32_t  fb_cursor_x = 0;
static uint32_t  fb_cursor_y = 0;

static void fb_putpixel(uint32_t x, uint32_t y, uint32_t color) {
    if (x >= fb_width || y >= fb_height) return;
    uint8_t *row = (uint8_t *)fb + y * fb_pitch;
    *(uint32_t *)(row + x * 4) = color;
}

static void fb_clear(uint32_t color) {
    for (uint32_t y = 0; y < fb_height; y++)
        for (uint32_t x = 0; x < fb_width; x++)
            fb_putpixel(x, y, color);
    fb_cursor_x = 0;
    fb_cursor_y = 0;
}

static void fb_scroll(void) {
    uint32_t line_h = FONT_H;
    if (fb_cursor_y + line_h <= fb_height) return;
    uint32_t *dst = fb;
    uint32_t *src = (uint32_t *)((uint8_t *)fb + line_h * fb_pitch);
    uint32_t words = (fb_height - line_h) * (fb_pitch / 4);
    for (uint32_t i = 0; i < words; i++) dst[i] = src[i];
    for (uint32_t y = fb_height - line_h; y < fb_height; y++)
        for (uint32_t x = 0; x < fb_width; x++)
            fb_putpixel(x, y, 0);
    fb_cursor_y -= line_h;
}

/* ---- BIOS font ---- */
static const uint8_t *font_rom = 0;

static void font_init(void) {
    uint64_t hhdm = get_hhdm_offset();
    font_rom = (const uint8_t *)(hhdm + FONT_ROM_PHYS);
}

static void draw_char(uint8_t c, uint32_t x, uint32_t y, uint32_t color) {
    const uint8_t *glyph = font_rom + (uint32_t)c * FONT_H;
    for (uint32_t gy = 0; gy < FONT_H; gy++) {
        uint8_t bits = glyph[gy];
        for (uint32_t gx = 0; gx < FONT_W; gx++) {
            if (bits & (0x80 >> gx))
                fb_putpixel(x + gx, y + gy, color);
        }
    }
}

void kprint(const char *s) {
    if (!fb || !font_rom) return;
    while (*s) {
        uint8_t c = (uint8_t)*s++;
        if (c == '\n') {
            fb_cursor_x = 0;
            fb_cursor_y += FONT_H;
            fb_scroll();
            continue;
        }
        if (c == '\r') { fb_cursor_x = 0; continue; }
        draw_char(c, fb_cursor_x, fb_cursor_y, 0xFFFFFF);
        fb_cursor_x += FONT_W;
        if (fb_cursor_x + FONT_W > fb_width) {
            fb_cursor_x = 0;
            fb_cursor_y += FONT_H;
            fb_scroll();
        }
    }
}

void kprintc(char c) {
    char buf[2] = { c, 0 };
    kprint(buf);
}

/* ---- PS/2 keyboard ---- */
#define PS2_DATA 0x60

static const char kbd_map[128] = {
    0,  27, '1','2','3','4','5','6','7','8','9','0','-','=', '\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,  'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,  '\\','z','x','c','v','b','n','m',',','.','/',
    0,  '*', 0,  ' ',
};

static volatile char kbd_buf[256];
static volatile int  kbd_head = 0;
static volatile int  kbd_tail = 0;

static void kbd_handler(void) {
    uint8_t sc = inb(PS2_DATA);
    if (sc & 0x80) return;
    char c = kbd_map[sc & 0x7F];
    if (!c) return;
    int next = (kbd_head + 1) % 256;
    if (next != kbd_tail) {
        kbd_buf[kbd_head] = c;
        kbd_head = next;
    }
}

char kinput(void) {
    while (kbd_head == kbd_tail) __asm__ volatile("hlt");
    char c = kbd_buf[kbd_tail];
    kbd_tail = (kbd_tail + 1) % 256;
    return c;
}

/* ---- IDT ---- */
struct idt_entry {
    uint16_t off_lo;
    uint16_t sel;
    uint8_t  ist;
    uint8_t  type;
    uint16_t off_mid;
    uint32_t off_hi;
    uint32_t zero;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

static struct idt_entry idt[256];
static struct idt_ptr   idtp;

extern void irq1_stub(void);

static void idt_set(int n, void (*handler)(void)) {
    uint64_t addr = (uint64_t)handler;
    idt[n].off_lo  = addr & 0xFFFF;
    idt[n].sel     = 0x08;
    idt[n].ist     = 0;
    idt[n].type    = 0x8E;
    idt[n].off_mid = (addr >> 16) & 0xFFFF;
    idt[n].off_hi  = (addr >> 32) & 0xFFFFFFFF;
    idt[n].zero    = 0;
}

static void idt_load(void) {
    idtp.limit = sizeof(idt) - 1;
    idtp.base  = (uint64_t)&idt;
    __asm__ volatile("lidt %0" : : "m"(idtp));
}

static void pic_remap(void) {
    outb(0x20, 0x11); outb(0xA0, 0x11);
    outb(0x21, 0x20); outb(0xA1, 0x28);
    outb(0x21, 0x04); outb(0xA1, 0x02);
    outb(0x21, 0x01); outb(0xA1, 0x01);
    outb(0x21, 0xFD); outb(0xA1, 0xFF);
}

void irq1_handler_c(void) {
    kbd_handler();
    outb(0x20, 0x20);
}

/* ---- Helpers ---- */
static int str_prefix(const char *s, const char *p) {
    while (*p) if (*s++ != *p++) return 0;
    return 1;
}

static void print_u32(uint32_t v) {
    char buf[11];
    int i = 0;
    if (v == 0) { kprintc('0'); return; }
    while (v > 0 && i < 10) { buf[i++] = '0' + (v % 10); v /= 10; }
    while (i--) kprintc(buf[i]);
}

/* ---- FatFs required ---- */
DWORD get_fattime(void) {
    return ((DWORD)(2025 - 1980) << 25)
         | ((DWORD)1  << 21)
         | ((DWORD)1  << 16);
}

/* ---- FatFs state ---- */
static FATFS fatfs;
static FIL   fil;
static int   fs_mounted = 0;
static char  file_buffer[512];

/* ---- Commands ---- */
static void cmd_help(void) {
    kprint("CardboardOS shell commands\n");
    kprint("----------------------------\n");
    kprint("  help            show this text\n");
    kprint("  clear           clear the screen\n");
    kprint("  echo <text>     print text back\n");
    kprint("  about           kernel info\n");
    kprint("\n");
    kprint("Storage:\n");
    kprint("  mount           mount FAT32 on drive 0\n");
    kprint("  umount          unmount drive 0\n");
    kprint("  ls              list root directory\n");
    kprint("  cat <file>      print file contents\n");
    kprint("  stat            filesystem total/free sectors\n");
    kprint("\n");
    kprint("Examples:\n");
    kprint("  mount\n");
    kprint("  ls\n");
    kprint("  cat HELLO.TXT\n");
}

static void cmd_mount(void) {
    if (fs_mounted) { kprint("already mounted\n"); return; }
    FRESULT fr = f_mount(&fatfs, "", 1);
    if (fr != FR_OK) {
        kprint("mount failed, FRESULT=");
        print_u32(fr);
        kprint("\n");
        return;
    }
    fs_mounted = 1;
    kprint("mounted drive 0\n");
}

static void cmd_umount(void) {
    if (!fs_mounted) { kprint("not mounted\n"); return; }
    f_mount(NULL, "", 0);
    fs_mounted = 0;
    kprint("unmounted\n");
}

static void cmd_ls(void) {
    if (!fs_mounted) { kprint("not mounted\n"); return; }
    DIR dir;
    FILINFO fno;
    FRESULT fr = f_opendir(&dir, "/");
    if (fr != FR_OK) {
        kprint("opendir failed, FRESULT=");
        print_u32(fr);
        kprint("\n");
        return;
    }
    for (;;) {
        fr = f_readdir(&dir, &fno);
        if (fr != FR_OK || fno.fname[0] == 0) break;
        if (fno.fattrib & AM_DIR) kprint("[DIR]  ");
        else                      kprint("[FILE] ");
        kprint(fno.fname);
        kprint("\n");
    }
    f_closedir(&dir);
}

static void cmd_cat(const char *filename) {
    if (!fs_mounted) { kprint("not mounted\n"); return; }
    if (*filename == 0) { kprint("usage: cat <file>\n"); return; }

    FRESULT fr = f_open(&fil, filename, FA_READ);
    if (fr != FR_OK) {
        kprint("open failed, FRESULT=");
        print_u32(fr);
        kprint("\n");
        return;
    }

    kprint("--- ");
    kprint(filename);
    kprint(" ---\n");

    for (;;) {
        UINT br = 0;
        fr = f_read(&fil, file_buffer, sizeof(file_buffer) - 1, &br);
        if (fr != FR_OK) { kprint("\nread error\n"); break; }
        if (br == 0) break;
        file_buffer[br] = 0;
        kprint(file_buffer);
    }
    kprint("\n--- end ---\n");
    f_close(&fil);
}

static void cmd_stat(void) {
    if (!fs_mounted) { kprint("not mounted\n"); return; }
    DWORD fre_clust = 0;
    FATFS *fs = &fatfs;
    FRESULT fr = f_getfree("", &fre_clust, &fs);
    if (fr != FR_OK) {
        kprint("getfree failed, FRESULT=");
        print_u32(fr);
        kprint("\n");
        return;
    }
    DWORD tot_sect = (fs->n_fatent - 2) * fs->csize;
    DWORD fre_sect = fre_clust * fs->csize;
    kprint("total sectors: "); print_u32(tot_sect); kprint("\n");
    kprint("free sectors:  "); print_u32(fre_sect); kprint("\n");
}

static void shell_line(char *buf, int len) {
    buf[len] = 0;
    if (len == 0) return;

    if      (str_prefix(buf, "help"))        cmd_help();
    else if (str_prefix(buf, "clear"))       fb_clear(0);
    else if (str_prefix(buf, "echo "))       { kprint(buf + 5); kprint("\n"); }
    else if (str_prefix(buf, "about"))       kprint("CardboardOS - a small kernel\n");
    else if (str_prefix(buf, "mount"))       cmd_mount();
    else if (str_prefix(buf, "umount"))      cmd_umount();
    else if (str_prefix(buf, "ls"))          cmd_ls();
    else if (str_prefix(buf, "cat "))        cmd_cat(buf + 4);
    else if (str_prefix(buf, "stat"))        cmd_stat();
    else {
        kprint("unknown command: ");
        kprint(buf);
        kprint("\n");
    }
}

/* ---- Entry ---- */
void kmain(void) {
    serial_init();
    serial_puts("\n=== CardboardOS ===\n");

    struct limine_framebuffer *lfb = get_framebuffer();
    if (!lfb) {
        serial_puts("no framebuffer\n");
        for (;;) __asm__ volatile("hlt");
    }

    fb        = (uint32_t *)lfb->address;
    fb_width  = lfb->width;
    fb_height = lfb->height;
    fb_pitch  = lfb->pitch;

    font_init();
    serial_puts("font rom: ");
    serial_hex((uint64_t)font_rom);
    serial_puts("\n");

    fb_clear(0x00000000);
    kprint("CardboardOS\n");
    kprint("type 'help' for commands\n\n");

    idt_set(0x21, irq1_stub);
    idt_load();
    pic_remap();
    __asm__ volatile("sti");

    kprint("> ");

    char line[128];
    int  len = 0;

    for (;;) {
        char c = kinput();
        if (c == '\n') {
            kprintc('\n');
            shell_line(line, len);
            len = 0;
            kprint("> ");
        } else if (c == '\b') {
            if (len > 0) { len--; kprintc('\b'); }
        } else if (c >= ' ' && len < 127) {
            line[len++] = c;
            kprintc(c);
        }
    }
}