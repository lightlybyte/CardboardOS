import os

kernellocation = os.path.join(os.getcwd(), "src", "kernel")
kernelfile = os.path.join(kernellocation, "kmain.c")
multibootfile = os.path.join(kernellocation, "boot.asm")
linkerfile = os.path.join(kernellocation, "linker.ld")
buildlocation = os.path.join(os.getcwd(), "build")

print("[ 1/3 ] Compiling OS Kernel...")
os.system(f"nasm -f elf32 {multibootfile} -o {buildlocation}/boot.o")
os.system(f"clang -target i386-elf -m32 -ffreestanding -fno-stack-protector -fno-builtin -c {kernelfile} -o {buildlocation}/kmain.o")
os.system(f"ld.lld -m elf_i386 -T {linkerfile} {buildlocation}/boot.o {buildlocation}/kmain.o -o {buildlocation}/cardboard.elf")

print("[ 2/3 ] Creating ISO Image... (For GRUB 0.97)")
os.system(f"mkdir -p {buildlocation}/iso/boot/grub")
os.system(f"cp {buildlocation}/cardboard.elf {buildlocation}/iso/boot/cardboard.elf")
os.system(f"echo 'set timeout=0\nset default=0\nmenuentry \"CardboardOS\" {{\n\tmultiboot /boot/cardboard.elf\n\tboot\n}}' > {buildlocation}/iso/boot/grub/grub.cfg")
os.system(f"mkisofs -R -b boot/grub/stage2_eltorito -no-emul-boot -boot-load-size 4 -A \"CardboardOS\" -input-charset utf8 -o {buildlocation}/cardboard.iso {buildlocation}/iso")

print("[ 3/3 ] Running OS in QEMU...")
os.system(f"qemu-system-i386 -cdrom {buildlocation}/cardboard.iso")
