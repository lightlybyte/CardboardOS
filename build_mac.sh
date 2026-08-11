#!/bin/bash

# build.sh - Linux/macOS build script
# Usage: ./build.sh [--clean] [--run] [--debug]

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Parse arguments
CLEAN=true
RUN=false
DEBUG=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --no-clean) CLEAN=false ;;
        --run) RUN=true ;;
        --debug) DEBUG=true ;;
        *) echo "Unknown option: $1"; exit 1 ;;
    esac
    shift
done

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo -e "${CYAN}=========================================="
echo "Building CardboardOS"
echo -e "==========================================${NC}"

# Create and clean build directory
mkdir -p build

if [ "$CLEAN" = true ]; then
    echo -e "${YELLOW}Cleaning build directory...${NC}"
    rm -f build/*
fi

# 1. Compile C kernel
echo -e "${YELLOW}[1/4] Compiling kmain.c...${NC}"
clang -m32 -ffreestanding -nostdlib -fno-pie -fno-stack-protector \
      -c src/kernel/kmain.c -o build/kernel.o -target i386-pc-none-elf

# 2. Link with LLD
echo -e "${YELLOW}[2/4] Linking kernel with LLD...${NC}"
lld -m elf_i386 -T src/linker.ld -o build/kernel.elf build/kernel.o

# Check symbols
echo -e "${YELLOW}[INFO] Checking symbols...${NC}"
llvm-nm build/kernel.elf | grep kmain || true

# Extract raw binary
echo -e "${YELLOW}[3/4] Extracting raw kernel binary...${NC}"
llvm-objcopy -O binary build/kernel.elf build/kernel.bin

# 3. Assemble bootloader
echo -e "${YELLOW}[4/4] Assembling bootloader...${NC}"
nasm -f bin src/bootloader/boot.asm -o build/bootloader.bin

# 4. Combine
echo -e "${YELLOW}Combining bootloader and kernel...${NC}"
cat build/bootloader.bin build/kernel.bin > build/os-image.bin

# Show size
if [[ "$OSTYPE" == "darwin"* ]]; then
    SIZE=$(stat -f%z build/os-image.bin)
else
    SIZE=$(stat -c%s build/os-image.bin)
fi

echo ""
echo -e "${GREEN}=========================================="
echo "Build complete!"
echo -e "==========================================${NC}"
echo "OS image: build/os-image.bin ($SIZE bytes)"
echo ""
echo -e "${YELLOW}Run with:${NC}"
echo "  qemu-system-i386 -drive format=raw,file=build/os-image.bin"
echo ""
echo -e "${YELLOW}Debug with:${NC}"
echo "  qemu-system-i386 -s -S -drive format=raw,file=build/os-image.bin"
echo "  gdb -ex \"target remote localhost:1234\" -ex \"symbol-file build/kernel.elf\""

if [ "$RUN" = true ]; then
    echo ""
    echo -e "${GREEN}Running OS...${NC}"
    qemu-system-i386 -drive format=raw,file=build/os-image.bin
fi

if [ "$DEBUG" = true ]; then
    echo ""
    echo -e "${GREEN}Starting debug session...${NC}"
    qemu-system-i386 -s -S -drive format=raw,file=build/os-image.bin &
    sleep 1
    gdb -ex "target remote localhost:1234" -ex "symbol-file build/kernel.elf"
fi