# CardboardOS

[![Build](https://img.shields.io/badge/build-passing-brightgreen)]()
[![Platform](https://img.shields.io/badge/platform-x86__64-blue)]()
[![Bootloader](https://img.shields.io/badge/bootloader-Limine-orange)]()
[![License](https://img.shields.io/badge/license-MIT-lightgrey)]()

CardboardOS is a hobbyist operating system for x86_64, built from scratch with a
freestanding kernel, a small shell, a user system, a bytecode language, and a
graphical toolkit linked in. It boots via Limine on BIOS and runs in QEMU.

---

## Current status

Everything listed under **Implemented** below compiles and links into the kernel.
Everything under **Planned** is designed but not yet built.

### Implemented

**Boot and platform**
- Limine bootloader (BIOS, El Torito CD)
- x86_64 long mode entry
- Freestanding C and C++ (no hosted libc)
- Linear framebuffer via Limine's protocol
- IDT with IRQ1 handling
- Programmable Interrupt Controller remap
- Serial console on COM1
- PS/2 keyboard with shift and caps lock

**Display**
- VGA 8x16 bitmap font embedded at build time
- Text rendering to a 32-bit linear framebuffer
- Scrolling, backspace, newline handling

**Filesystem**
- FatFs integration over IDE PIO
- Sector reads and writes through `diskio.c`
- Directory listing, file read, file write, rename, delete, mkdir, rmdir
- Path resolution with `.` and `..`
- Current working directory tracking
- Output redirection (`>` and `>>`) for `echo`

**Users**
- `user.card` file on the FAT volume
- Format: `username:password:role`
- Roles: `user` and `admin`
- Login prompt with hidden password entry
- Commands: `passwd`, `createuser`, `deluser`, `setrole`
- Admin-only enforcement and last-admin protection

**Shell**
- Prompt: `[user]@cardboard-$ `
- Command history (16 entries)
- Command dispatch for ~40 builtins
- Three-page manual via `help 1`, `help 2`, `help 3`
- Per-command usage via `man <command>`

**NotC language**
- Source files with `.nc` extension
- Compiler: lexer, parser, code generator
- Output format: `.ca` bytecode files
- Bytecode VM in the kernel
- Current grammar: `main: [ write("...") ]`
- Shell commands: `notc <file.nc>` compiles, `run <file.ca>` executes

**Graphics**
- GuiLite (header-only C++ library) linked into the kernel
- Port hooks: `delay_ms`, `get_frame_buffer`, `draw_pixel`
- C++ freestanding support: `-fno-exceptions -fno-rtti`
- Runtime stubs: `__cxa_pure_virtual`, `operator new`/`delete`
- Hosted libc stubs: `printf`, `sprintf`, `malloc`, `abs`, and others

### Planned

- ext2, ext3, ext4 support via lwext4
- exFAT, FAT12, FAT16 read/write
- AHCI storage backend (replacing IDE PIO)
- NVMe storage backend
- PCI enumeration
- NotC extensions: variables, arithmetic, conditionals, loops, functions
- NotC standard library and native call ABI
- GuiLite sample applications wired into the shell
- Multi-tasking (currently single-threaded, interrupts only)
- Heap allocator with real free-list (currently bump-only)

---

## Building

### Prerequisites

| Tool | Purpose | Notes |
|------|---------|-------|
| `nasm` | Assemble boot stub and IRQ stub | Any recent version |
| `clang` | Compile C and C++ | Must support `-target x86_64-elf` |
| `ld.lld` | Link the kernel ELF | Ships with LLVM |
| `xorriso` | Build the bootable ISO | Windows build available |
| `qemu-system-x86_64` | Run the kernel | Any recent version |
| Limine | Bootloader binaries | Download from the Limine project |

Limine files needed:
- `limine.exe` (installer)
- `limine-bios.sys`
- `limine-bios-cd.bin`

Place them in `C:\limine\` (or edit the `LIMINE_DIR` constant in `build.py`).

### Build and run
python build.py
OR
build.bat


The script:

1. Converts `src/kernel/VGA8.F16` into a C array in `src/kernel/vga_font.c`.
2. Assembles `boot.asm` and `irq1.asm` with NASM.
3. Compiles the C sources with Clang for `x86_64-elf`.
4. Compiles the C++ sources with `-fno-exceptions -fno-rtti -std=c++17`.
5. Links everything into `build/cardboard.elf`.
6. Places the kernel and Limine files into an ISO tree.
7. Builds `build/cardboard.iso` with xorriso.
8. Patches the ISO with `limine bios-install`.
9. Launches QEMU with the ISO as a CD and `fat32.img` as an IDE disk.

### Disk image

The kernel expects a FAT32 volume as the second IDE drive. Create one with a
64 MB or 128 MB disk image, formatted as FAT32, and place it at `fat32.img` in
the project root.

Optionally include:

- `/user.card` — `username:password:role` entries, one per line
- `/hello.nc` — a NotC source file

If `user.card` is missing, the kernel falls back to `root:root:admin`.

---

## Layout
CardboardOS/
├── build.py Build script
├── README.md This file
├── fat32.img FAT32 disk image (not committed)
├── include/
│ ├── diskio.h FatFs disk I/O layer
│ ├── ff.h FatFs main header
│ ├── ffconf.h FatFs configuration
│ ├── float.h Freestanding float limits
│ ├── GuiLite.h Graphics library (header-only)
│ ├── limits.h Freestanding integer limits
│ ├── limine.h Limine protocol header
│ ├── math.h Freestanding math stubs
│ ├── new C++ placement new
│ ├── notc.h NotC compiler and VM
│ ├── stdarg.h Variadic macros
│ ├── stdbool.h Boolean type
│ ├── stddef.h size_t, NULL
│ ├── stdint.h Fixed-width integer types
│ ├── stdio.h printf, sprintf stubs
│ ├── stdlib.h malloc, abs stubs
│ └── string.h memcpy, strlen, and friends
└── src/
└── kernel/
├── boot.asm Kernel entry point
├── cxx_support.cpp C++ runtime stubs
├── diskio.c FatFs to IDE glue
├── ff.c FatFs core
├── gui.cpp GuiLite port hooks
├── ide.c IDE PIO driver
├── irq1.asm Keyboard interrupt stub
├── kmain.c Kernel main, shell, users
├── libc_stubs.c Hosted libc replacements
├── limine_requests.c Framebuffer and HHDM requests
├── linker.ld Kernel linker script
├── notc.c NotC compiler and VM
├── notc_hook.c NotC write() output hook
├── string.c String and memory functions
├── VGA8.F16 8x16 VGA font bitmap
└── vga_font.c Generated from VGA8.F16 at build


---

## Shell reference

Type `help` for the manual index. Type `help 1`, `help 2`, or `help 3` for the
full pages. Type `man <command>` for usage on a specific command.

### Navigation and files

| Command | Usage |
|---------|-------|
| `ls` | List current directory |
| `pwd` | Print working directory |
| `cd <dir>` | Change directory |
| `mkdir <dir>` | Create directory |
| `rmdir <dir>` | Remove empty directory |
| `rm <path>` | Delete a file |
| `cat <file>` | Print file contents |
| `head <file>` | Print first 10 lines |
| `tail <file>` | Print last 10 lines |
| `wc <file>` | Count lines, words, bytes |
| `hexdump <file>` | Hex and ASCII dump |
| `touch <file>` | Create empty file |
| `write <file> <text>` | Overwrite file with text |
| `append <file> <text>` | Append text to file |
| `cp <src> <dst>` | Copy a file |
| `mv <src> <dst>` | Rename or move |
| `echo <text> > <file>` | Redirect echo to file |
| `echo <text> >> <file>` | Append echo to file |

### Disk

| Command | Usage |
|---------|-------|
| `mount` | Mount FAT32 on drive 0 |
| `umount` | Unmount drive 0 |
| `stat` | Show filesystem size and free space |

### System

| Command | Usage |
|---------|-------|
| `date` | Show current date and time |
| `uname` | Show system name |
| `whoami` | Show current user and role |
| `id` | Show user and group IDs |
| `env` | Show environment variables |
| `history` | Show command history |
| `which <cmd>` | Locate a command |
| `type <cmd>` | Alias of `which` |
| `clear` | Clear the screen |
| `about` | Kernel information |
| `exit` | Exit hint |

### User management

| Command | Usage | Access |
|---------|-------|--------|
| `passwd` | Change your own password | Any user |
| `createuser <user> <pass> <role>` | Add a user | Admin only |
| `deluser <user>` | Remove a user | Admin only |
| `setrole <user> <role>` | Change a user's role | Admin only |

### NotC

| Command | Usage |
|---------|-------|
| `notc <file.nc>` | Compile NotC source to `<file>.ca` |
| `run <file.ca>` | Execute compiled NotC |

---

## NotC language

NotC is a small language compiled to a bytecode format called `.ca`. The current
grammar supports only `write` statements inside a `main` block.
