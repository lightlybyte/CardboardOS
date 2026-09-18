import os
import shutil
import subprocess
import sys

root        = os.getcwd()
kernel_dir  = os.path.join(root, "src", "kernel")
include_dir = os.path.join(root, "include")
build_dir   = os.path.join(root, "build")
iso_dir     = os.path.join(build_dir, "iso")
boot_dir    = os.path.join(iso_dir, "boot")

LIMINE_DIR  = r"C:\limine"
LIMINE_EXE  = os.path.join(LIMINE_DIR, "limine.exe")
LIMINE_SYS  = os.path.join(LIMINE_DIR, "limine-bios.sys")
LIMINE_CD   = os.path.join(LIMINE_DIR, "limine-bios-cd.bin")

NASM    = shutil.which("nasm")            or "nasm"
CLANG   = shutil.which("clang")           or "clang"
LD_LLD  = shutil.which("ld.lld")          or shutil.which("ld.lld.exe")
XORRISO = shutil.which("xorriso")         or "xorriso"
QEMU    = shutil.which("qemu-system-x86_64") or "qemu-system-x86_64"

if not LD_LLD:
    print("ERROR: ld.lld not found on PATH.")
    sys.exit(1)

kernel_srcs = [
    "kmain.c",
    "diskio.c",
    "ide.c",
    "ff.c",
    "string.c",
    "vga_font.c",
    "limine_requests.c",
]

boot_src    = os.path.join(kernel_dir, "boot.asm")
irq1_src    = os.path.join(kernel_dir, "irq1.asm")
linker_ld   = os.path.join(kernel_dir, "linker.ld")
font_bin    = os.path.join(kernel_dir, "VGA8.F16")
font_c      = os.path.join(kernel_dir, "vga_font.c")

boot_obj    = os.path.join(build_dir, "boot.o")
irq1_obj    = os.path.join(build_dir, "irq1.o")
kernel_elf  = os.path.join(build_dir, "cardboard.elf")
iso_path    = os.path.join(build_dir, "cardboard.iso")
disk_path   = os.path.join(root, "fat32.img")

if os.path.exists(iso_dir):
    shutil.rmtree(iso_dir)
os.makedirs(build_dir, exist_ok=True)
os.makedirs(boot_dir, exist_ok=True)


def run(cmd, cwd=None):
    print("    $ " + " ".join(str(c) for c in cmd))
    result = subprocess.run(cmd, cwd=cwd)
    if result.returncode != 0:
        print(f"    command failed with code {result.returncode}")
        sys.exit(1)


print("[ 0/5 ] Converting VGA8.F16 to C source...")
with open(font_bin, "rb") as f:
    data = f.read()

if len(data) != 4096:
    print(f"ERROR: VGA8.F16 is {len(data)} bytes, expected 4096")
    sys.exit(1)

with open(font_c, "w") as f:
    f.write("#include <stdint.h>\n\n")
    f.write("const uint8_t vga_font_8x16[4096] = {\n")
    for i in range(0, 4096, 16):
        f.write("    ")
        f.write(", ".join(f"0x{b:02X}" for b in data[i:i+16]))
        f.write(",\n")
    f.write("};\n")

print(f"    wrote {font_c}")

print("[ 1/5 ] Compiling OS Kernel (x86-64)...")
run([NASM, "-f", "elf64", boot_src, "-o", boot_obj])
run([NASM, "-f", "elf64", irq1_src, "-o", irq1_obj])

objs = [boot_obj, irq1_obj]
for src in kernel_srcs:
    s = os.path.join(kernel_dir, src)
    o = os.path.join(build_dir, src.replace(".c", ".o"))
    objs.append(o)
    run([
        CLANG,
        "-target", "x86_64-elf",
        "-m64",
        "-ffreestanding",
        "-fno-stack-protector",
        "-fno-builtin",
        "-mno-red-zone",
        "-I", include_dir,
        "-I", kernel_dir,
        "-c", s,
        "-o", o,
    ])

run([LD_LLD, "-m", "elf_x86_64", "-T", linker_ld, *objs, "-o", kernel_elf])

print("[ 2/5 ] Placing files in ISO tree...")
shutil.copy(kernel_elf, os.path.join(boot_dir, "cardboard.elf"))
shutil.copy(LIMINE_SYS, os.path.join(boot_dir, "limine-bios.sys"))
shutil.copy(LIMINE_CD,  os.path.join(boot_dir, "limine-bios-cd.bin"))

cfg_contents = (
    "timeout: 0\n"
    "/CardboardOS\n"
    "    protocol: limine\n"
    "    path: boot():/boot/cardboard.elf\n"
)

with open(os.path.join(iso_dir, "limine.conf"), "w") as f:
    f.write(cfg_contents)
with open(os.path.join(boot_dir, "limine.conf"), "w") as f:
    f.write(cfg_contents)

print("[ 3/5 ] Building ISO...")
run([
    XORRISO, "-as", "mkisofs",
    "-R", "-r", "-J",
    "-V", "CARDBOARDOS",
    "-b", "boot/limine-bios-cd.bin",
    "-no-emul-boot",
    "-boot-load-size", "4",
    "-boot-info-table",
    "-o", "cardboard.iso",
    "iso",
], cwd=build_dir)

print("[ 4/5 ] Installing Limine BIOS stage...")
run([LIMINE_EXE, "bios-install", "--force", "cardboard.iso"], cwd=build_dir)

print("[ 5/5 ] Running OS in QEMU (x86-64)...")
run([
    QEMU,
    "-cdrom", iso_path,
    "-drive", f"file={disk_path},format=raw,if=ide,index=0",
    "-serial", "stdio",
])