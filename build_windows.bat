@echo off
setlocal enabledelayedexpansion

echo ==========================================
echo Building CardboardOS
echo ==========================================

REM Get the directory where this script is located
set SCRIPT_DIR=%~dp0
cd /d %SCRIPT_DIR%

REM Create build directory if it doesn't exist
if not exist build mkdir build

REM Clean build directory
echo Cleaning build directory...
del /q build\* 2>nul

REM 1. Compile C kernel
echo [1/4] Compiling kmain.c...
clang -m32 -ffreestanding -nostdlib -fno-pie -fno-stack-protector -c src\kernel\kmain.c -o build\kernel.o -target i386-pc-none-elf
if errorlevel 1 (
    echo Compilation failed!
    exit /b 1
)

REM 2. Link with LLD
echo [2/4] Linking kernel with LLD...
lld -m elf_i386 -T src\linker.ld -o build\kernel.elf build\kernel.o
if errorlevel 1 (
    echo Linking failed!
    exit /b 1
)

REM Check symbols
echo [INFO] Checking symbols...
llvm-nm build\kernel.elf | findstr kmain

REM Extract raw binary
echo [3/4] Extracting raw kernel binary...
llvm-objcopy -O binary build\kernel.elf build\kernel.bin

REM 3. Assemble bootloader
echo [4/4] Assembling bootloader...
nasm -f bin src\bootloader\boot.asm -o build\bootloader.bin
if errorlevel 1 (
    echo Assembly failed!
    exit /b 1
)

REM 4. Combine
echo Combining bootloader and kernel...
copy /b build\bootloader.bin + build\kernel.bin build\os-image.bin

REM Show size
for %%A in (build\os-image.bin) do set SIZE=%%~zA
echo.
echo ==========================================
echo Build complete!
echo ==========================================
echo OS image: build\os-image.bin (!SIZE! bytes)
echo.
echo Run with:
echo   qemu-system-i386 -drive format=raw,file=build\os-image.bin
echo.
echo Debug with:
echo   qemu-system-i386 -s -S -drive format=raw,file=build\os-image.bin