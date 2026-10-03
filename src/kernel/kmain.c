#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include "ff.h"
#include "diskio.h"
#include "limine.h"
#include "notc.h"

extern const uint8_t vga_font_8x16[4096];
extern struct limine_framebuffer *get_framebuffer(void);

#define FONT_W 8
#define FONT_H 16

#define MAX_USERS    32
#define MAX_USER_LEN 32

/* ---- Port I/O ---- */
static inline uint8_t inb(uint16_t port) {
    uint8_t v;
    __asm__ volatile("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}
static inline void outb(uint16_t port, uint8_t v) {
    __asm__ volatile("outb %0, %1" : : "a"(v), "Nd"(port));
}

/* ---- Serial ---- */
static void serial_init(void) {
    outb(0x3F9, 0x00); outb(0x3FB, 0x80); outb(0x3F8, 0x01);
    outb(0x3F9, 0x00); outb(0x3FB, 0x03); outb(0x3FA, 0xC7); outb(0x3FC, 0x0B);
}
static void serial_putc(char c)        { outb(0x3F8, (uint8_t)c); }
static void serial_puts(const char *s) { while (*s) serial_putc(*s++); }

/* ---- Framebuffer ---- */
uint32_t *fb = 0;
uint32_t fb_width = 0, fb_height = 0, fb_pitch = 0;
uint32_t fb_cursor_x = 0, fb_cursor_y = 0;

static void fb_putpixel(uint32_t x, uint32_t y, uint32_t color) {
    if (x >= fb_width || y >= fb_height) return;
    uint8_t *row = (uint8_t *)fb + y * fb_pitch;
    *(uint32_t *)(row + x * 4) = color;
}

static void fb_clear(uint32_t color) {
    for (uint32_t y = 0; y < fb_height; y++)
        for (uint32_t x = 0; x < fb_width; x++)
            fb_putpixel(x, y, color);
    fb_cursor_x = 0; fb_cursor_y = 0;
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

static void draw_char(uint8_t c, uint32_t x, uint32_t y, uint32_t color) {
    const uint8_t *glyph = vga_font_8x16 + (uint32_t)c * FONT_H;
    for (uint32_t gy = 0; gy < FONT_H; gy++) {
        uint8_t bits = glyph[gy];
        for (uint32_t gx = 0; gx < FONT_W; gx++)
            if (bits & (0x80 >> gx))
                fb_putpixel(x + gx, y + gy, color);
    }
}

static void kprint_nolock(const char *s) {
    if (!fb) return;
    while (*s) {
        uint8_t c = (uint8_t)*s++;
        if (c == '\n') { fb_cursor_x = 0; fb_cursor_y += FONT_H; fb_scroll(); continue; }
        if (c == '\r') { fb_cursor_x = 0; continue; }
        draw_char(c, fb_cursor_x, fb_cursor_y, 0xFFFFFF);
        fb_cursor_x += FONT_W;
        if (fb_cursor_x + FONT_W > fb_width) {
            fb_cursor_x = 0; fb_cursor_y += FONT_H; fb_scroll();
        }
    }
}

void kprint(const char *s) { __asm__ volatile("cli"); kprint_nolock(s); __asm__ volatile("sti"); }
void kprintc(char c)       { char b[2] = { c, 0 }; kprint(b); }

/* ---- Keyboard ---- */
#define PS2_DATA 0x60

static const char kbd_map_lower[128] = {
    0,   27, '1','2','3','4','5','6','7','8','9','0','-','=', '\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,   'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,   '\\','z','x','c','v','b','n','m',',','.','/',
    0,   '*', 0,  ' ',
    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,
};

static const char kbd_map_upper[128] = {
    0,   27, '!','@','#','$','%','^','&','*','(',')','_','+', '\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0,   'A','S','D','F','G','H','J','K','L',':','"','~',
    0,   '|','Z','X','C','V','B','N','M','<','>','?',
    0,   '*', 0,  ' ',
    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,
};

#define SC_LSHIFT   0x2A
#define SC_RSHIFT   0x36
#define SC_CAPSLOCK 0x3A

static volatile char kbd_buf[256];
static volatile int  kbd_head = 0;
static volatile int  kbd_tail = 0;
static volatile int  shift_down = 0;
static volatile int  caps_lock  = 0;

static int is_shifted(int sc) {
    int shifted = shift_down;
    if (caps_lock) {
        if (sc >= 0x10 && sc <= 0x19) shifted = !shifted;
        if (sc >= 0x1E && sc <= 0x26) shifted = !shifted;
        if (sc >= 0x2C && sc <= 0x32) shifted = !shifted;
    }
    return shifted;
}

static void kbd_handler(void) {
    uint8_t sc = inb(PS2_DATA);
    int released = sc & 0x80;
    int code = sc & 0x7F;

    if (code == SC_LSHIFT || code == SC_RSHIFT) {
        shift_down = !released;
        return;
    }
    if (code == SC_CAPSLOCK && !released) {
        caps_lock = !caps_lock;
        return;
    }
    if (released) return;

    char c = is_shifted(code) ? kbd_map_upper[code] : kbd_map_lower[code];
    if (!c) return;

    int next = (kbd_head + 1) % 256;
    if (next != kbd_tail) { kbd_buf[kbd_head] = c; kbd_head = next; }
}

char kinput(void) {
    while (kbd_head == kbd_tail) __asm__ volatile("hlt");
    char c = kbd_buf[kbd_tail];
    kbd_tail = (kbd_tail + 1) % 256;
    return c;
}

/* ---- IDT ---- */
struct idt_entry {
    uint16_t off_lo, sel; uint8_t ist, type;
    uint16_t off_mid; uint32_t off_hi, zero;
} __attribute__((packed));
struct idt_ptr { uint16_t limit; uint64_t base; } __attribute__((packed));
static struct idt_entry idt[256];
static struct idt_ptr idtp;
static uint16_t kernel_cs = 0x08;
extern void irq1_stub(void);

static void idt_set(int n, void (*h)(void)) {
    uint64_t a = (uint64_t)h;
    idt[n].off_lo = a & 0xFFFF;
    idt[n].sel = kernel_cs;
    idt[n].ist = 0;
    idt[n].type = 0x8E;
    idt[n].off_mid = (a >> 16) & 0xFFFF;
    idt[n].off_hi = (a >> 32) & 0xFFFFFFFF;
    idt[n].zero = 0;
}
static void idt_load(void) {
    idtp.limit = sizeof(idt) - 1;
    idtp.base = (uint64_t)&idt;
    __asm__ volatile("lidt %0" : : "m"(idtp));
}
static void pic_remap(void) {
    outb(0x20, 0x11); outb(0xA0, 0x11);
    outb(0x21, 0x20); outb(0xA1, 0x28);
    outb(0x21, 0x04); outb(0xA1, 0x02);
    outb(0x21, 0x01); outb(0xA1, 0x01);
    outb(0x21, 0xFD); outb(0xA1, 0xFF);
}
void irq1_handler_c(void) { kbd_handler(); outb(0x20, 0x20); }

/* ---- Helpers ---- */
static int str_prefix(const char *s, const char *p) {
    while (*p) if (*s++ != *p++) return 0;
    return 1;
}
static void print_u32(uint32_t v) {
    char b[11]; int i = 0;
    if (v == 0) { kprintc('0'); return; }
    while (v > 0 && i < 10) { b[i++] = '0' + (v % 10); v /= 10; }
    while (i--) kprintc(b[i]);
}

/* ---- FatFs ---- */
DWORD get_fattime(void) {
    return ((DWORD)(2025 - 1980) << 25) | ((DWORD)1 << 21) | ((DWORD)1 << 16);
}

static FATFS fatfs;
static FIL fil;
static int fs_mounted = 0;
static char file_buffer[512];
static char cwd[256] = "/";

static void path_join(const char *base, const char *in, char *out, size_t outsz) {
    char tmp[512];
    if (in[0] == '/') {
        strncpy(tmp, in, sizeof(tmp) - 1);
        tmp[sizeof(tmp) - 1] = 0;
    } else {
        size_t n = strlen(base);
        if (n > 0 && base[n - 1] == '/')
            snprintf(tmp, sizeof(tmp), "%s%s", base, in);
        else
            snprintf(tmp, sizeof(tmp), "%s/%s", base, in);
    }

    char *segs[128];
    int nseg = 0;
    char *p = tmp;
    while (*p) {
        while (*p == '/') p++;
        if (!*p) break;
        char *start = p;
        while (*p && *p != '/') p++;
        char save = *p; *p = 0;
        if (strcmp(start, ".") == 0) { }
        else if (strcmp(start, "..") == 0) { if (nseg > 0) nseg--; }
        else if (nseg < 128) segs[nseg++] = start;
        if (save) p++;
    }

    size_t off = 0;
    out[off++] = '/';
    for (int i = 0; i < nseg; i++) {
        size_t l = strlen(segs[i]);
        if (off + l + 2 >= outsz) break;
        memcpy(out + off, segs[i], l);
        off += l;
        if (i < nseg - 1) out[off++] = '/';
    }
    out[off] = 0;
}

/* ---- Users ---- */
struct user_entry {
    char name[MAX_USER_LEN];
    char pass[MAX_USER_LEN];
    char role[MAX_USER_LEN];
};
static struct user_entry users[MAX_USERS];
static int  user_count = 0;
static char current_user[MAX_USER_LEN] = "";
static int  current_is_admin = 0;

static int find_user(const char *name) {
    for (int i = 0; i < user_count; i++)
        if (strcmp(users[i].name, name) == 0) return i;
    return -1;
}

static void process_user_line(const char *line) {
    if (user_count >= MAX_USERS) return;
    char buf[128];
    strncpy(buf, line, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = 0;

    char *c1 = strchr(buf, ':');
    if (!c1) return;
    *c1 = 0;
    char *c2 = strchr(c1 + 1, ':');
    if (!c2) return;
    *c2 = 0;

    const char *name = buf;
    const char *pass = c1 + 1;
    const char *role = c2 + 1;

    if (!name[0] || !pass[0] || !role[0]) return;
    if (strlen(name) >= MAX_USER_LEN) return;
    if (strlen(pass) >= MAX_USER_LEN) return;
    if (strlen(role) >= MAX_USER_LEN) return;
    if (strcmp(role, "user") != 0 && strcmp(role, "admin") != 0) return;

    strncpy(users[user_count].name, name, MAX_USER_LEN - 1);
    users[user_count].name[MAX_USER_LEN - 1] = 0;
    strncpy(users[user_count].pass, pass, MAX_USER_LEN - 1);
    users[user_count].pass[MAX_USER_LEN - 1] = 0;
    strncpy(users[user_count].role, role, MAX_USER_LEN - 1);
    users[user_count].role[MAX_USER_LEN - 1] = 0;
    user_count++;
}

static int parse_user_card(void) {
    FIL f;
    FRESULT fr = f_open(&f, "/user.card", FA_READ);
    if (fr != FR_OK) return -1;

    user_count = 0;
    char line[128];
    int pos = 0;

    for (;;) {
        char c;
        UINT br = 0;
        fr = f_read(&f, &c, 1, &br);
        if (fr != FR_OK || br == 0) break;
        if (c == '\n' || c == '\r') {
            if (pos > 0) { line[pos] = 0; process_user_line(line); pos = 0; }
        } else if (pos < (int)sizeof(line) - 1) {
            line[pos++] = c;
        }
    }
    if (pos > 0) { line[pos] = 0; process_user_line(line); }
    f_close(&f);
    return 0;
}

static int save_user_card(void) {
    FIL f;
    FRESULT fr = f_open(&f, "/user.card", FA_WRITE | FA_CREATE_ALWAYS);
    if (fr != FR_OK) return -1;

    for (int i = 0; i < user_count; i++) {
        char line[160];
        snprintf(line, sizeof(line), "%s:%s:%s\n",
                 users[i].name, users[i].pass, users[i].role);
        UINT bw = 0;
        fr = f_write(&f, line, strlen(line), &bw);
        if (fr != FR_OK) { f_close(&f); return -1; }
    }
    f_close(&f);
    return 0;
}

static int authenticate(void) {
    char name[MAX_USER_LEN];
    char pass[MAX_USER_LEN];
    int len;

    for (int attempt = 0; attempt < 3; attempt++) {
        kprint("login: ");
        len = 0;
        for (;;) {
            char c = kinput();
            if (c == '\n') { kprintc('\n'); break; }
            if (c == '\b') { if (len > 0) { len--; kprintc('\b'); } }
            else if (c >= ' ' && len < MAX_USER_LEN - 1) { name[len++] = c; kprintc(c); }
        }
        name[len] = 0;

        kprint("password: ");
        len = 0;
        for (;;) {
            char c = kinput();
            if (c == '\n') { kprintc('\n'); break; }
            if (c == '\b') { if (len > 0) len--; }
            else if (c >= ' ' && len < MAX_USER_LEN - 1) pass[len++] = c;
        }
        pass[len] = 0;

        for (int i = 0; i < user_count; i++) {
            if (strcmp(users[i].name, name) == 0 &&
                strcmp(users[i].pass, pass) == 0) {
                strncpy(current_user, name, MAX_USER_LEN - 1);
                current_user[MAX_USER_LEN - 1] = 0;
                current_is_admin = (strcmp(users[i].role, "admin") == 0);
                return 0;
            }
        }
        kprint("login incorrect\n");
    }
    return -1;
}

/* ---- Prompt ---- */
static void print_prompt(void) {
    kprint("[");
    kprint(current_user[0] ? current_user : "guest");
    kprint("]@cardboard-$ ");
}

/* ---- Help ---- */
static void cmd_help_page(int page) {
    if (page == 1) {
        kprint("CardboardOS manual - page 1/3\n");
        kprint("==============================\n");
        kprint("  help             show page list\n");
        kprint("  help <1-3>       show a specific page\n");
        kprint("  man <command>    show usage for a command\n");
        kprint("  clear            clear the screen\n");
        kprint("  echo <text>      print text back\n");
        kprint("  about            kernel info\n");
        kprint("  exit             exit hint\n");
    } else if (page == 2) {
        kprint("CardboardOS manual - page 2/3 (files)\n");
        kprint("======================================\n");
        kprint("  ls, pwd, cd, mkdir, rmdir\n");
        kprint("  cat, head, tail, wc, hexdump\n");
        kprint("  touch, write, append, cp, mv, rm\n");
        kprint("  echo <t> > <f>      redirect to file\n");
        kprint("  echo <t> >> <f>     append to file\n");
    } else if (page == 3) {
        kprint("CardboardOS manual - page 3/3 (system)\n");
        kprint("=======================================\n");
        kprint("  mount, umount, stat\n");
        kprint("  date, uname, whoami, id, env\n");
        kprint("  history, which, type, man\n");
        kprint("  passwd, createuser, deluser, setrole\n");
        kprint("  notc <file.nc>      compile NotC\n");
        kprint("  run <file.ca>       run compiled NotC\n");
    } else {
        kprint("no such manual page\n");
    }
}

static void cmd_help(const char *arg) {
    while (*arg == ' ') arg++;
    if (*arg == 0) {
        kprint("CardboardOS manual\n");
        kprint("------------------\n");
        kprint("  help 1     general\n");
        kprint("  help 2     filesystem\n");
        kprint("  help 3     system and NotC\n");
        kprint("\n  man <cmd>  one-line usage\n");
        return;
    }
    if (arg[0] >= '1' && arg[0] <= '3' && arg[1] == 0)
        cmd_help_page(arg[0] - '0');
    else
        kprint("usage: help, help 1, help 2, help 3\n");
}

/* ---- man ---- */
struct man_entry { const char *name; const char *usage; };
static const struct man_entry man_pages[] = {
    { "help",       "help | help 1-3        show manual pages" },
    { "man",        "man <cmd>              show usage for a command" },
    { "clear",      "clear                  clear the screen" },
    { "echo",       "echo <text>            print text" },
    { "about",      "about                  kernel info" },
    { "ls",         "ls                     list directory" },
    { "pwd",        "pwd                    print working directory" },
    { "cd",         "cd <dir>               change directory" },
    { "mkdir",      "mkdir <dir>            create directory" },
    { "rmdir",      "rmdir <dir>            remove empty directory" },
    { "rm",         "rm <path>              delete a file" },
    { "cat",        "cat <file>             print file contents" },
    { "head",       "head <file>            first 10 lines" },
    { "tail",       "tail <file>            last 10 lines" },
    { "wc",         "wc <file>              line/word/byte count" },
    { "hexdump",    "hexdump <file>         hex + ascii dump" },
    { "touch",      "touch <file>           create empty file" },
    { "write",      "write <file> <text>    overwrite file" },
    { "append",     "append <file> <text>   append text to file" },
    { "cp",         "cp <src> <dst>         copy a file" },
    { "mv",         "mv <src> <dst>         rename or move" },
    { "mount",      "mount                  mount FAT32 on drive 0" },
    { "umount",     "umount                 unmount drive 0" },
    { "stat",       "stat                   filesystem stats" },
    { "date",       "date                   show current date/time" },
    { "uname",      "uname                  show system name" },
    { "whoami",     "whoami                 current user + role" },
    { "id",         "id                     user and group IDs" },
    { "env",        "env                    list environment" },
    { "history",    "history                command history" },
    { "which",      "which <cmd>            locate a command" },
    { "type",       "type <cmd>             alias of which" },
    { "passwd",     "passwd                 change your password" },
    { "createuser", "createuser <u> <p> <role>   add a user" },
    { "deluser",    "deluser <user>              remove a user" },
    { "setrole",    "setrole <user> <role>       change role" },
    { "notc",       "notc <file.nc>              compile NotC source" },
    { "run",        "run <file.ca>               run compiled NotC" },
    { "exit",       "exit                        exit hint" },
    { 0, 0 }
};

static void cmd_man(const char *arg) {
    while (*arg == ' ') arg++;
    if (*arg == 0) { kprint("usage: man <command>\n"); return; }
    char name[32]; size_t i = 0;
    while (arg[i] && arg[i] != ' ' && i < sizeof(name) - 1) { name[i] = arg[i]; i++; }
    name[i] = 0;
    for (int j = 0; man_pages[j].name; j++) {
        if (strcmp(name, man_pages[j].name) == 0) {
            kprint("Usage: "); kprint(man_pages[j].usage); kprint("\n");
            return;
        }
    }
    kprint("man: no entry for '"); kprint(name); kprint("'\n");
}

/* ---- which ---- */
static const char *builtin_names[] = {
    "help","man","clear","echo","about","ls","pwd","cd","mkdir","rmdir",
    "rm","cat","head","tail","wc","hexdump","touch","write","append",
    "cp","mv","mount","umount","stat","date","uname","whoami","id",
    "env","history","which","type","passwd","createuser","deluser",
    "setrole","notc","run","exit", 0
};
static int is_builtin(const char *cmd, size_t len) {
    for (int i = 0; builtin_names[i]; i++)
        if (strlen(builtin_names[i]) == len &&
            strncmp(builtin_names[i], cmd, len) == 0) return 1;
    return 0;
}
static void cmd_which(const char *arg) {
    while (*arg == ' ') arg++;
    if (*arg == 0) { kprint("usage: which <cmd>\n"); return; }
    const char *p = arg;
    while (*p && *p != ' ') p++;
    size_t len = (size_t)(p - arg);
    if (is_builtin(arg, len)) { kprint(arg); kprint(": shell builtin\n"); }
    else                       { kprint(arg); kprint(": not found\n"); }
}

/* ---- History ---- */
#define HIST_MAX 16
static char hist[HIST_MAX][128];
static int hist_count = 0, hist_next = 0;
static void history_add(const char *line) {
    strncpy(hist[hist_next], line, 127);
    hist[hist_next][127] = 0;
    hist_next = (hist_next + 1) % HIST_MAX;
    if (hist_count < HIST_MAX) hist_count++;
}
static void cmd_history(void) {
    int start = (hist_count < HIST_MAX) ? 0 : hist_next;
    for (int i = 0; i < hist_count; i++) {
        int idx = (start + i) % HIST_MAX;
        char tmp[16];
        snprintf(tmp, sizeof(tmp), "%3d  ", i + 1);
        kprint(tmp); kprint(hist[idx]); kprint("\n");
    }
}

/* ---- Navigation ---- */
static void cmd_pwd(void) { kprint(cwd); kprint("\n"); }
static void cmd_cd(const char *arg) {
    while (*arg == ' ') arg++;
    if (!fs_mounted) { kprint("not mounted\n"); return; }
    const char *target = (*arg == 0) ? "/" : arg;
    char resolved[256];
    path_join(cwd, target, resolved, sizeof(resolved));
    DIR dir;
    FRESULT fr = f_opendir(&dir, resolved);
    if (fr != FR_OK) { kprint("cd: no such directory: "); kprint(arg); kprint("\n"); return; }
    f_closedir(&dir);
    strncpy(cwd, resolved, sizeof(cwd) - 1);
    cwd[sizeof(cwd) - 1] = 0;
}

/* ---- Listing ---- */
static void cmd_ls(void) {
    if (!fs_mounted) { kprint("not mounted\n"); return; }
    DIR dir; FILINFO fno;
    FRESULT fr = f_opendir(&dir, cwd);
    if (fr != FR_OK) { kprint("opendir failed\n"); return; }
    for (;;) {
        fr = f_readdir(&dir, &fno);
        if (fr != FR_OK || fno.fname[0] == 0) break;
        if (fno.fattrib & AM_DIR) kprint("[DIR]  ");
        else                      kprint("[FILE] ");
        kprint(fno.fname); kprint("\n");
    }
    f_closedir(&dir);
}

/* ---- File operations ---- */
static void cmd_mkdir(const char *arg) {
    while (*arg == ' ') arg++;
    if (!fs_mounted || *arg == 0) { kprint("usage: mkdir <dir>\n"); return; }
    char r[256]; path_join(cwd, arg, r, sizeof(r));
    FRESULT fr = f_mkdir(r);
    if (fr != FR_OK) { kprint("mkdir: FRESULT="); print_u32(fr); kprint("\n"); }
}
static void cmd_rm(const char *arg) {
    while (*arg == ' ') arg++;
    if (!fs_mounted || *arg == 0) { kprint("usage: rm <path>\n"); return; }
    char r[256]; path_join(cwd, arg, r, sizeof(r));
    FRESULT fr = f_unlink(r);
    if (fr != FR_OK) { kprint("rm: FRESULT="); print_u32(fr); kprint("\n"); }
}
static void cmd_rmdir(const char *arg) {
    while (*arg == ' ') arg++;
    if (!fs_mounted || *arg == 0) { kprint("usage: rmdir <dir>\n"); return; }
    char r[256]; path_join(cwd, arg, r, sizeof(r));
    FRESULT fr = f_unlink(r);
    if (fr != FR_OK) { kprint("rmdir: FRESULT="); print_u32(fr); kprint("\n"); }
}
static void cmd_touch(const char *arg) {
    while (*arg == ' ') arg++;
    if (!fs_mounted || *arg == 0) { kprint("usage: touch <file>\n"); return; }
    char r[256]; path_join(cwd, arg, r, sizeof(r));
    FIL f;
    FRESULT fr = f_open(&f, r, FA_WRITE | FA_OPEN_ALWAYS);
    if (fr != FR_OK) { kprint("touch: FRESULT="); print_u32(fr); kprint("\n"); return; }
    f_close(&f);
}
static void cmd_write(const char *arg) {
    while (*arg == ' ') arg++;
    if (!fs_mounted || *arg == 0) { kprint("usage: write <file> <text>\n"); return; }
    char fn[128]; size_t i = 0;
    while (*arg && *arg != ' ' && i < sizeof(fn) - 1) fn[i++] = *arg++;
    fn[i] = 0;
    if (*arg == 0) { kprint("usage: write <file> <text>\n"); return; }
    while (*arg == ' ') arg++;
    const char *text = arg;
    char r[256]; path_join(cwd, fn, r, sizeof(r));
    FIL f;
    FRESULT fr = f_open(&f, r, FA_WRITE | FA_CREATE_ALWAYS);
    if (fr != FR_OK) { kprint("write: FRESULT="); print_u32(fr); kprint("\n"); return; }
    UINT bw = 0;
    f_write(&f, text, strlen(text), &bw);
    f_write(&f, "\n", 1, &bw);
    f_close(&f);
}
static void cmd_append(const char *arg) {
    while (*arg == ' ') arg++;
    if (!fs_mounted || *arg == 0) { kprint("usage: append <file> <text>\n"); return; }
    char fn[128]; size_t i = 0;
    while (*arg && *arg != ' ' && i < sizeof(fn) - 1) fn[i++] = *arg++;
    fn[i] = 0;
    if (*arg == 0) { kprint("usage: append <file> <text>\n"); return; }
    while (*arg == ' ') arg++;
    const char *text = arg;
    char r[256]; path_join(cwd, fn, r, sizeof(r));
    FIL f;
    FRESULT fr = f_open(&f, r, FA_WRITE | FA_OPEN_APPEND);
    if (fr != FR_OK) { kprint("append: FRESULT="); print_u32(fr); kprint("\n"); return; }
    UINT bw = 0;
    f_write(&f, text, strlen(text), &bw);
    f_write(&f, "\n", 1, &bw);
    f_close(&f);
}
static void cmd_cp(const char *arg) {
    while (*arg == ' ') arg++;
    if (!fs_mounted || *arg == 0) { kprint("usage: cp <src> <dst>\n"); return; }
    char src[128], dst[128]; size_t i = 0;
    while (*arg && *arg != ' ' && i < sizeof(src) - 1) src[i++] = *arg++;
    src[i] = 0;
    while (*arg == ' ') arg++;
    i = 0;
    while (*arg && *arg != ' ' && i < sizeof(dst) - 1) dst[i++] = *arg++;
    dst[i] = 0;
    if (src[0] == 0 || dst[0] == 0) { kprint("usage: cp <src> <dst>\n"); return; }
    char rs[256], rd[256];
    path_join(cwd, src, rs, sizeof(rs));
    path_join(cwd, dst, rd, sizeof(rd));
    FIL fi, fo;
    FRESULT fr = f_open(&fi, rs, FA_READ);
    if (fr != FR_OK) { kprint("cp: src\n"); return; }
    fr = f_open(&fo, rd, FA_WRITE | FA_CREATE_ALWAYS);
    if (fr != FR_OK) { kprint("cp: dst\n"); f_close(&fi); return; }
    for (;;) {
        UINT br = 0, bw = 0;
        fr = f_read(&fi, file_buffer, sizeof(file_buffer), &br);
        if (fr != FR_OK || br == 0) break;
        fr = f_write(&fo, file_buffer, br, &bw);
        if (fr != FR_OK || bw != br) break;
    }
    f_close(&fi); f_close(&fo);
}
static void cmd_mv(const char *arg) {
    while (*arg == ' ') arg++;
    if (!fs_mounted || *arg == 0) { kprint("usage: mv <src> <dst>\n"); return; }
    char src[128], dst[128]; size_t i = 0;
    while (*arg && *arg != ' ' && i < sizeof(src) - 1) src[i++] = *arg++;
    src[i] = 0;
    while (*arg == ' ') arg++;
    i = 0;
    while (*arg && *arg != ' ' && i < sizeof(dst) - 1) dst[i++] = *arg++;
    dst[i] = 0;
    if (src[0] == 0 || dst[0] == 0) { kprint("usage: mv <src> <dst>\n"); return; }
    char rs[256], rd[256];
    path_join(cwd, src, rs, sizeof(rs));
    path_join(cwd, dst, rd, sizeof(rd));
    FRESULT fr = f_rename(rs, rd);
    if (fr != FR_OK) { kprint("mv: FRESULT="); print_u32(fr); kprint("\n"); }
}
static void cmd_cat(const char *arg) {
    while (*arg == ' ') arg++;
    if (!fs_mounted || *arg == 0) { kprint("usage: cat <file>\n"); return; }
    char r[256]; path_join(cwd, arg, r, sizeof(r));
    FRESULT fr = f_open(&fil, r, FA_READ);
    if (fr != FR_OK) { kprint("cat: FRESULT="); print_u32(fr); kprint("\n"); return; }
    for (;;) {
        UINT br = 0;
        fr = f_read(&fil, file_buffer, sizeof(file_buffer) - 1, &br);
        if (fr != FR_OK || br == 0) break;
        file_buffer[br] = 0;
        kprint(file_buffer);
    }
    f_close(&fil);
}
static void cmd_head(const char *arg) {
    while (*arg == ' ') arg++;
    if (!fs_mounted || *arg == 0) { kprint("usage: head <file>\n"); return; }
    char r[256]; path_join(cwd, arg, r, sizeof(r));
    FIL f;
    FRESULT fr = f_open(&f, r, FA_READ);
    if (fr != FR_OK) { kprint("head: FRESULT="); print_u32(fr); kprint("\n"); return; }
    int lines = 0;
    for (;;) {
        UINT br = 0;
        fr = f_read(&f, file_buffer, sizeof(file_buffer) - 1, &br);
        if (fr != FR_OK || br == 0) break;
        file_buffer[br] = 0;
        for (UINT i = 0; i < br; i++) {
            if (file_buffer[i] == '\n') { lines++; if (lines >= 10) { f_close(&f); return; } }
            kprintc(file_buffer[i]);
        }
    }
    f_close(&f);
}
static void cmd_tail(const char *arg) {
    while (*arg == ' ') arg++;
    if (!fs_mounted || *arg == 0) { kprint("usage: tail <file>\n"); return; }
    char r[256]; path_join(cwd, arg, r, sizeof(r));
    FIL f;
    FRESULT fr = f_open(&f, r, FA_READ);
    if (fr != FR_OK) { kprint("tail: FRESULT="); print_u32(fr); kprint("\n"); return; }
    static char buf[4096];
    FSIZE_t size = f_size(&f);
    if (size > (FSIZE_t)sizeof(buf) - 1) f_lseek(&f, size - (sizeof(buf) - 1));
    UINT br = 0;
    f_read(&f, buf, sizeof(buf) - 1, &br);
    buf[br] = 0;
    int seen = 0, start = 0;
    for (int i = (int)br - 2; i >= 0; i--) {
        if (buf[i] == '\n') { seen++; if (seen == 10) { start = i + 1; break; } }
    }
    for (int i = start; i < (int)br; i++) kprintc(buf[i]);
    f_close(&f);
}
static void cmd_wc(const char *arg) {
    while (*arg == ' ') arg++;
    if (!fs_mounted || *arg == 0) { kprint("usage: wc <file>\n"); return; }
    char r[256]; path_join(cwd, arg, r, sizeof(r));
    FIL f;
    FRESULT fr = f_open(&f, r, FA_READ);
    if (fr != FR_OK) { kprint("wc: FRESULT="); print_u32(fr); kprint("\n"); return; }
    uint32_t lines = 0, words = 0, bytes = 0;
    int in_word = 0;
    for (;;) {
        UINT br = 0;
        fr = f_read(&f, file_buffer, sizeof(file_buffer), &br);
        if (fr != FR_OK || br == 0) break;
        bytes += br;
        for (UINT i = 0; i < br; i++) {
            char c = file_buffer[i];
            if (c == '\n') lines++;
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') in_word = 0;
            else if (!in_word) { in_word = 1; words++; }
        }
    }
    f_close(&f);
    print_u32(lines); kprint(" ");
    print_u32(words); kprint(" ");
    print_u32(bytes); kprint(" ");
    kprint(arg); kprint("\n");
}
static void cmd_hexdump(const char *arg) {
    while (*arg == ' ') arg++;
    if (!fs_mounted || *arg == 0) { kprint("usage: hexdump <file>\n"); return; }
    char r[256]; path_join(cwd, arg, r, sizeof(r));
    FIL f;
    FRESULT fr = f_open(&f, r, FA_READ);
    if (fr != FR_OK) { kprint("hexdump: FRESULT="); print_u32(fr); kprint("\n"); return; }
    uint32_t offset = 0;
    char tmp[32];
    for (;;) {
        UINT br = 0;
        fr = f_read(&f, file_buffer, 16, &br);
        if (fr != FR_OK || br == 0) break;
        snprintf(tmp, sizeof(tmp), "%08x  ", offset);
        kprint(tmp);
        for (UINT i = 0; i < 16; i++) {
            if (i < br) { snprintf(tmp, sizeof(tmp), "%02x ", (unsigned char)file_buffer[i]); kprint(tmp); }
            else kprint("   ");
            if (i == 7) kprint(" ");
        }
        kprint(" |");
        for (UINT i = 0; i < br; i++) {
            char c = file_buffer[i];
            kprintc((c >= 0x20 && c < 0x7F) ? c : '.');
        }
        kprint("|\n");
        offset += br;
        if (br < 16) break;
    }
    f_close(&f);
}

/* ---- Disk ---- */
static void cmd_mount(void) {
    if (fs_mounted) { kprint("already mounted\n"); return; }
    FRESULT fr = f_mount(&fatfs, "", 1);
    if (fr != FR_OK) { kprint("mount failed, FRESULT="); print_u32(fr); kprint("\n"); return; }
    fs_mounted = 1;
    kprint("mounted drive 0\n");
}
static void cmd_umount(void) {
    if (!fs_mounted) { kprint("not mounted\n"); return; }
    f_mount(NULL, "", 0);
    fs_mounted = 0;
    kprint("unmounted\n");
}
static void cmd_stat(void) {
    if (!fs_mounted) { kprint("not mounted\n"); return; }
    DWORD fre_clust = 0; FATFS *fs = &fatfs;
    FRESULT fr = f_getfree("", &fre_clust, &fs);
    if (fr != FR_OK) { kprint("getfree failed\n"); return; }
    DWORD tot = (fs->n_fatent - 2) * fs->csize;
    DWORD fre = fre_clust * fs->csize;
    kprint("total sectors: "); print_u32(tot); kprint("\n");
    kprint("free sectors:  "); print_u32(fre); kprint("\n");
}

/* ---- System ---- */
static void cmd_date(void)   { kprint("2025-01-01 00:00:00 UTC\n"); }
static void cmd_uname(void)  { kprint("CardboardOS 0.1 x86_64\n"); }
static void cmd_id(void)     { kprint("uid=0(root) gid=0(root)\n"); }
static void cmd_env(void) {
    kprint("HOME=/\nPATH=/bin\nSHELL=/bin/cbsh\nPWD=");
    kprint(cwd); kprint("\nUSER="); kprint(current_user); kprint("\n");
}
static void cmd_whoami(void) {
    if (current_user[0] == 0) { kprint("(not logged in)\n"); return; }
    kprint(current_user);
    kprint(current_is_admin ? " (admin)\n" : " (user)\n");
}

/* ---- User management ---- */
static void cmd_createuser(const char *arg) {
    if (!current_is_admin) { kprint("createuser: permission denied\n"); return; }
    char name[MAX_USER_LEN], pass[MAX_USER_LEN], role[MAX_USER_LEN];
    const char *p = arg;
    while (*p == ' ') p++;
    size_t i = 0;
    while (*p && *p != ' ' && i < MAX_USER_LEN - 1) name[i++] = *p++;
    name[i] = 0;
    while (*p == ' ') p++;
    i = 0;
    while (*p && *p != ' ' && i < MAX_USER_LEN - 1) pass[i++] = *p++;
    pass[i] = 0;
    while (*p == ' ') p++;
    i = 0;
    while (*p && *p != ' ' && i < MAX_USER_LEN - 1) role[i++] = *p++;
    role[i] = 0;
    if (!name[0] || !pass[0] || !role[0]) { kprint("usage: createuser <u> <p> <user|admin>\n"); return; }
    if (strcmp(role, "user") != 0 && strcmp(role, "admin") != 0) {
        kprint("createuser: role must be 'user' or 'admin'\n"); return;
    }
    if (find_user(name) >= 0) { kprint("createuser: user already exists\n"); return; }
    if (user_count >= MAX_USERS) { kprint("createuser: user table full\n"); return; }

    strncpy(users[user_count].name, name, MAX_USER_LEN - 1); users[user_count].name[MAX_USER_LEN - 1] = 0;
    strncpy(users[user_count].pass, pass, MAX_USER_LEN - 1); users[user_count].pass[MAX_USER_LEN - 1] = 0;
    strncpy(users[user_count].role, role, MAX_USER_LEN - 1); users[user_count].role[MAX_USER_LEN - 1] = 0;
    user_count++;
    if (save_user_card() != 0) kprint("createuser: failed to save\n");
    else                       kprint("user created\n");
}
static void cmd_deluser(const char *arg) {
    if (!current_is_admin) { kprint("deluser: permission denied\n"); return; }
    while (*arg == ' ') arg++;
    char name[MAX_USER_LEN];
    size_t i = 0;
    while (arg[i] && arg[i] != ' ' && i < MAX_USER_LEN - 1) { name[i] = arg[i]; i++; }
    name[i] = 0;
    if (name[0] == 0) { kprint("usage: deluser <user>\n"); return; }
    if (strcmp(name, current_user) == 0) { kprint("deluser: cannot delete the current user\n"); return; }
    int idx = find_user(name);
    if (idx < 0) { kprint("deluser: no such user\n"); return; }
    int admin_count = 0;
    for (int i = 0; i < user_count; i++)
        if (strcmp(users[i].role, "admin") == 0) admin_count++;
    if (strcmp(users[idx].role, "admin") == 0 && admin_count <= 1) {
        kprint("deluser: cannot remove the last admin\n"); return;
    }
    for (int j = idx; j < user_count - 1; j++) users[j] = users[j + 1];
    user_count--;
    if (save_user_card() != 0) kprint("deluser: failed to save\n");
    else                       kprint("user deleted\n");
}
static void cmd_setrole(const char *arg) {
    if (!current_is_admin) { kprint("setrole: permission denied\n"); return; }
    char name[MAX_USER_LEN], role[MAX_USER_LEN];
    while (*arg == ' ') arg++;
    size_t i = 0;
    while (*arg && *arg != ' ' && i < MAX_USER_LEN - 1) name[i++] = *arg++;
    name[i] = 0;
    while (*arg == ' ') arg++;
    i = 0;
    while (*arg && *arg != ' ' && i < MAX_USER_LEN - 1) role[i++] = *arg++;
    role[i] = 0;
    if (!name[0] || !role[0]) { kprint("usage: setrole <user> <user|admin>\n"); return; }
    if (strcmp(role, "user") != 0 && strcmp(role, "admin") != 0) {
        kprint("setrole: role must be 'user' or 'admin'\n"); return;
    }
    int idx = find_user(name);
    if (idx < 0) { kprint("setrole: no such user\n"); return; }
    if (strcmp(users[idx].role, role) == 0) { kprint("setrole: role unchanged\n"); return; }
    if (strcmp(users[idx].role, "admin") == 0 && strcmp(role, "user") == 0) {
        int admin_count = 0;
        for (int i = 0; i < user_count; i++)
            if (strcmp(users[i].role, "admin") == 0) admin_count++;
        if (admin_count <= 1) { kprint("setrole: cannot demote the last admin\n"); return; }
    }
    strncpy(users[idx].role, role, MAX_USER_LEN - 1);
    users[idx].role[MAX_USER_LEN - 1] = 0;
    if (strcmp(name, current_user) == 0)
        current_is_admin = (strcmp(role, "admin") == 0);
    if (save_user_card() != 0) kprint("setrole: failed to save\n");
    else                       kprint("role updated\n");
}
static void cmd_passwd(const char *arg) {
    (void)arg;
    int idx = find_user(current_user);
    if (idx < 0) { kprint("passwd: not logged in\n"); return; }
    kprint("new password: ");
    char np[MAX_USER_LEN];
    size_t i = 0;
    for (;;) {
        char c = kinput();
        if (c == '\n') { kprintc('\n'); break; }
        if (c == '\b') { if (i > 0) i--; }
        else if (c >= ' ' && i < MAX_USER_LEN - 1) np[i++] = c;
    }
    np[i] = 0;
    if (np[0] == 0) { kprint("passwd: empty password\n"); return; }
    strncpy(users[idx].pass, np, MAX_USER_LEN - 1);
    users[idx].pass[MAX_USER_LEN - 1] = 0;
    if (save_user_card() != 0) kprint("passwd: failed to save\n");
    else                       kprint("password changed\n");
}

/* ---- NotC ---- */
static void cmd_notc(const char *arg) {
    while (*arg == ' ') arg++;
    if (*arg == 0) { kprint("usage: notc <file.nc>\n"); return; }
    if (!fs_mounted) { kprint("not mounted\n"); return; }

    char src_path[256];
    path_join(cwd, arg, src_path, sizeof(src_path));

    FIL f;
    FRESULT fr = f_open(&f, src_path, FA_READ);
    if (fr != FR_OK) { kprint("notc: cannot open source\n"); return; }

    static char source[8192];
    UINT br = 0;
    fr = f_read(&f, source, sizeof(source) - 1, &br);
    f_close(&f);
    if (fr != FR_OK) { kprint("notc: read error\n"); return; }
    source[br] = 0;

    static uint8_t ca[16384];
    size_t ca_len = 0;
    if (notc_compile(source, ca, sizeof(ca), &ca_len) != 0) {
        kprint("notc: compile failed\n");
        return;
    }

    char ca_path[256];
    strncpy(ca_path, src_path, sizeof(ca_path) - 1);
    ca_path[sizeof(ca_path) - 1] = 0;
    size_t plen = strlen(ca_path);
    if (plen > 3 && strcmp(ca_path + plen - 3, ".nc") == 0)
        strcpy(ca_path + plen - 3, ".ca");
    else
        strncat(ca_path, ".ca", sizeof(ca_path) - strlen(ca_path) - 1);

    fr = f_open(&f, ca_path, FA_WRITE | FA_CREATE_ALWAYS);
    if (fr != FR_OK) { kprint("notc: cannot open output\n"); return; }
    UINT bw = 0;
    f_write(&f, ca, ca_len, &bw);
    f_close(&f);

    kprint("notc: compiled to ");
    kprint(ca_path);
    kprint("\n");
}

static void cmd_run(const char *arg) {
    while (*arg == ' ') arg++;
    if (*arg == 0) { kprint("usage: run <file.ca>\n"); return; }
    if (!fs_mounted) { kprint("not mounted\n"); return; }

    char path[256];
    path_join(cwd, arg, path, sizeof(path));

    FIL f;
    FRESULT fr = f_open(&f, path, FA_READ);
    if (fr != FR_OK) { kprint("run: cannot open file\n"); return; }

    static uint8_t data[16384];
    UINT br = 0;
    fr = f_read(&f, data, sizeof(data), &br);
    f_close(&f);
    if (fr != FR_OK) { kprint("run: read error\n"); return; }

    static struct notc_program prog;
    if (notc_load(data, br, &prog) != 0) {
        kprint("run: not a valid .ca file\n");
        return;
    }
    if (notc_run(&prog) != 0)
        kprint("run: execution error\n");
}

/* ---- Redirect ---- */
static void write_redirect(const char *filename, const char *text, int append) {
    char r[256];
    path_join(cwd, filename, r, sizeof(r));
    FIL f;
    FRESULT fr = f_open(&f, r, append ? (FA_WRITE | FA_OPEN_APPEND)
                                      : (FA_WRITE | FA_CREATE_ALWAYS));
    if (fr != FR_OK) { kprint("redirect: FRESULT="); print_u32(fr); kprint("\n"); return; }
    UINT bw = 0;
    f_write(&f, text, strlen(text), &bw);
    f_write(&f, "\n", 1, &bw);
    f_close(&f);
}

/* ---- Dispatcher ---- */
static void shell_line(char *buf, int len) {
    buf[len] = 0;
    if (len == 0) return;
    history_add(buf);

    char *gt = 0; int append = 0;
    for (int i = 0; i < len - 1; i++) {
        if (buf[i] == '>' && buf[i+1] == '>') { gt = &buf[i]; append = 1; break; }
        if (buf[i] == '>') { gt = &buf[i]; break; }
    }
    if (gt) {
        char *file = gt + (append ? 2 : 1);
        while (*file == ' ') file++;
        char *e = file + strlen(file);
        while (e > file && (e[-1] == ' ' || e[-1] == '\n')) e--;
        *e = 0;
        char *cmd_end = gt;
        while (cmd_end > buf && cmd_end[-1] == ' ') cmd_end--;
        *cmd_end = 0;
        if (str_prefix(buf, "echo ")) write_redirect(file, buf + 5, append);
        else kprint("redirection only supported for 'echo'\n");
        return;
    }

    if      (str_prefix(buf, "help"))         cmd_help(buf + 4);
    else if (str_prefix(buf, "man "))         cmd_man(buf + 4);
    else if (str_prefix(buf, "clear"))        fb_clear(0);
    else if (str_prefix(buf, "echo "))        { kprint(buf + 5); kprint("\n"); }
    else if (str_prefix(buf, "about"))        kprint("CardboardOS - a small kernel\n");
    else if (str_prefix(buf, "mount"))        cmd_mount();
    else if (str_prefix(buf, "umount"))       cmd_umount();
    else if (str_prefix(buf, "ls"))           cmd_ls();
    else if (str_prefix(buf, "pwd"))          cmd_pwd();
    else if (str_prefix(buf, "cd"))           cmd_cd(buf + 2);
    else if (str_prefix(buf, "mkdir "))       cmd_mkdir(buf + 6);
    else if (str_prefix(buf, "rmdir "))       cmd_rmdir(buf + 6);
    else if (str_prefix(buf, "rm "))          cmd_rm(buf + 3);
    else if (str_prefix(buf, "touch "))       cmd_touch(buf + 6);
    else if (str_prefix(buf, "write "))       cmd_write(buf + 6);
    else if (str_prefix(buf, "append "))      cmd_append(buf + 7);
    else if (str_prefix(buf, "cp "))          cmd_cp(buf + 3);
    else if (str_prefix(buf, "mv "))          cmd_mv(buf + 3);
    else if (str_prefix(buf, "cat "))         cmd_cat(buf + 4);
    else if (str_prefix(buf, "head "))        cmd_head(buf + 5);
    else if (str_prefix(buf, "tail "))        cmd_tail(buf + 5);
    else if (str_prefix(buf, "wc "))          cmd_wc(buf + 3);
    else if (str_prefix(buf, "hexdump "))     cmd_hexdump(buf + 8);
    else if (str_prefix(buf, "stat"))         cmd_stat();
    else if (str_prefix(buf, "date"))         cmd_date();
    else if (str_prefix(buf, "uname"))        cmd_uname();
    else if (str_prefix(buf, "whoami"))       cmd_whoami();
    else if (str_prefix(buf, "id"))           cmd_id();
    else if (str_prefix(buf, "env"))          cmd_env();
    else if (str_prefix(buf, "history"))      cmd_history();
    else if (str_prefix(buf, "which "))       cmd_which(buf + 6);
    else if (str_prefix(buf, "type "))        cmd_which(buf + 5);
    else if (str_prefix(buf, "passwd"))       cmd_passwd(buf + 6);
    else if (str_prefix(buf, "createuser "))  cmd_createuser(buf + 11);
    else if (str_prefix(buf, "deluser "))     cmd_deluser(buf + 8);
    else if (str_prefix(buf, "setrole "))     cmd_setrole(buf + 8);
    else if (str_prefix(buf, "notc "))        cmd_notc(buf + 5);
    else if (str_prefix(buf, "run "))         cmd_run(buf + 4);
    else if (str_prefix(buf, "exit"))         kprint("Use QEMU: Ctrl+A X\n");
    else {
        kprint("unknown command: "); kprint(buf); kprint("\n");
        kprint("Type 'help' for a manual.\n");
    }
}

/* ---- Entry ---- */
void kmain(void) {
    serial_init();
    serial_puts("\n=== CardboardOS ===\n");

    uint16_t cs;
    __asm__ volatile("mov %%cs, %0" : "=r"(cs));
    kernel_cs = cs;

    struct limine_framebuffer *lfb = get_framebuffer();
    if (!lfb) { serial_puts("no framebuffer\n"); for (;;) __asm__ volatile("hlt"); }

    fb = (uint32_t *)lfb->address;
    fb_width = lfb->width; fb_height = lfb->height; fb_pitch = lfb->pitch;

    fb_clear(0x00000000);
    kprint("CardboardOS\n\n");

    idt_set(0x21, irq1_stub);
    idt_load();
    pic_remap();
    __asm__ volatile("sti");

    kprint("mounting drive 0...\n");
    cmd_mount();
    if (!fs_mounted) {
        kprint("no filesystem - halting\n");
        for (;;) __asm__ volatile("hlt");
    }

    if (parse_user_card() != 0 || user_count == 0) {
        kprint("no user.card - defaulting to root:root:admin\n");
        strncpy(users[0].name, "root", MAX_USER_LEN - 1);
        strncpy(users[0].pass, "root", MAX_USER_LEN - 1);
        strncpy(users[0].role, "admin", MAX_USER_LEN - 1);
        users[0].name[MAX_USER_LEN - 1] = 0;
        users[0].pass[MAX_USER_LEN - 1] = 0;
        users[0].role[MAX_USER_LEN - 1] = 0;
        user_count = 1;
    }

    if (authenticate() != 0) {
        kprint("authentication failed - halting\n");
        for (;;) __asm__ volatile("hlt");
    }
    kprint("Welcome, ");
    kprint(current_user);
    kprint(current_is_admin ? " (admin)\n\n" : " (user)\n\n");

    print_prompt();

    char line[128];
    int  len = 0;

    for (;;) {
        char c = kinput();
        if (c == '\n') {
            kprintc('\n');
            shell_line(line, len);
            len = 0;
            print_prompt();
        } else if (c == '\b') {
            if (len > 0) {
                len--;
                __asm__ volatile("cli");
                fb_cursor_x -= FONT_W;
                for (uint32_t gy = 0; gy < FONT_H; gy++)
                    for (uint32_t gx = 0; gx < FONT_W; gx++)
                        fb_putpixel(fb_cursor_x + gx, fb_cursor_y + gy, 0);
                __asm__ volatile("sti");
            }
        } else if (c >= ' ' && len < 127) {
            line[len++] = c;
            kprintc(c);
        }
    }
}