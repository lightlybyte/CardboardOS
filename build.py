import os
import shutil

root           = os.getcwd()
kernel_dir     = os.path.join(root, "src", "kernel")
build_dir      = os.path.join(root, "build")
iso_dir        = os.path.join(build_dir, "iso")
grub_dir       = os.path.join(iso_dir, "boot", "grub")

kmain_src      = os.path.join(kernel_dir, "kmain.c")
boot_src       = os.path.join(kernel_dir, "boot.asm")
linker_script  = os.path.join(kernel_dir, "linker.ld")

boot_obj       = os.path.join(build_dir, "boot.o")
kmain_obj      = os.path.join(build_dir, "kmain.o")
kernel_elf     = os.path.join(build_dir, "cardboard.elf")
iso_path       = os.path.join(build_dir, "cardboard.iso")

os.makedirs(build_dir, exist_ok=True)
os.makedirs(grub_dir, exist_ok=True)

print("[ 1/3 ] Compiling OS Kernel...")
os.system(f'nasm -f elf32 "{boot_src}" -o "{boot_obj}"')
os.system(f'clang -target i386-elf -m32 -ffreestanding -fno-stack-protector '
          f'-fno-builtin -c "{kmain_src}" -o "{kmain_obj}"')
os.system(f'ld.lld -m elf_i386 -T "{linker_script}" "{boot_obj}" "{kmain_obj}" '
          f'-o "{kernel_elf}"')

print("[ 2/3 ] Creating ISO Image...")
shutil.copy(kernel_elf, os.path.join(iso_dir, "boot", "cardboard.elf"))

# menu.lst for GRUB 0.97 (NOT grub.cfg)
menu_lst = (
    "default 0\n"
    "timeout 0\n"
    "title CardboardOS\n"
    "root (cd)\n"
    "kernel /boot/cardboard.elf\n"
)
with open(os.path.join(grub_dir, "menu.lst"), "w") as f:
    f.write(menu_lst)

# You must supply stage2_eltorito from your GRUB 0.97 install.
stage2_src = os.path.join(kernel_dir, "stage2_eltorito")
stage2_dst = os.path.join(grub_dir, "stage2_eltorito")
shutil.copy(stage2_src, stage2_dst)

os.system(
    f'mkisofs -R -b boot/grub/stage2_eltorito -no-emul-boot '
    f'-boot-load-size 4 -A "CardboardOS" -o "{iso_path}" "{iso_dir}"'
)

print("[ 3/3 ] Running OS in QEMU...")
os.system(f'qemu-system-i386 -cdrom "{iso_path}"')