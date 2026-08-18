# ==========================================
# CardboardOS Build Script (MULTIPLATFORMED)
# ==========================================

import sys, os, tarfile

"""
EXAMPLE GRUB FILE

iso\
    boot\
        grub\
            menu.lst
            stage2_eltorito
        kernel.elf
"""

if sys.argv[1] == None:
    print("""
============================
Building CardboardOS
============================

""")
    print("[1] - Build Build Folder Structure")
    file = tarfile.open("prebuild.tar.gz") 
    file.extractall("build")
    file.close()

    print("[2] - Build Kernel")
    os.system("nasm -f elf32 src\\boot\\boot.s")

    print("[3] - Linking The Kernel")
    os.system("ld.lld -T link.ld -m elf_i386 src\\boot\\boot.o -o kernel.elf")
