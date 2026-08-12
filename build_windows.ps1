# build.ps1 - Using ld.lld
Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "Building CardboardOS with ld.lld" -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan

# Create build directory
New-Item -ItemType Directory -Force -Path build | Out-Null
Remove-Item -Force build\* -ErrorAction SilentlyContinue

# 1. Compile C kernel with Clang
Write-Host "[1/4] Compiling kmain.c with Clang..." -ForegroundColor Yellow
clang -m32 -ffreestanding -nostdlib -fno-pie -fno-stack-protector -fno-builtin -c src\kernel\kmain.c -o build\kernel.o -target i386-pc-none-elf
if ($LASTEXITCODE -ne 0) { 
    Write-Host "Compilation failed!" -ForegroundColor Red
    exit 1 
}

# Check the object file
Write-Host "[INFO] Object file symbols:" -ForegroundColor Yellow
llvm-nm build\kernel.o | Select-String "kmain"

# 2. Link with ld.lld
Write-Host "[2/4] Linking kernel with ld.lld..." -ForegroundColor Yellow
ld.lld -m elf_i386 -T src\linker.ld -o build\kernel.elf build\kernel.o --no-dynamic-linker
if ($LASTEXITCODE -ne 0) { 
    Write-Host "Linking failed!" -ForegroundColor Red
    exit 1 
}

# Check the ELF file
Write-Host "[INFO] ELF symbols:" -ForegroundColor Yellow
llvm-nm build\kernel.elf | Select-String "kmain"

# 3. Extract binary
Write-Host "[3/4] Extracting kernel binary..." -ForegroundColor Yellow
llvm-objcopy -O binary build\kernel.elf build\kernel.bin
if ($LASTEXITCODE -ne 0) { 
    Write-Host "Binary extraction failed!" -ForegroundColor Red
    exit 1 
}

# Check binary size
$kernelSize = (Get-Item build\kernel.bin).Length
Write-Host "[INFO] Kernel size: $kernelSize bytes" -ForegroundColor Yellow

# 4. Assemble bootloader
Write-Host "[4/4] Assembling bootloader..." -ForegroundColor Yellow
nasm -f bin src\bootloader\boot.asm -o build\bootloader.bin
if ($LASTEXITCODE -ne 0) { 
    Write-Host "Assembly failed!" -ForegroundColor Red
    exit 1 
}

# 5. Create floppy image
Write-Host "Creating floppy image..." -ForegroundColor Yellow
$floppy = New-Object byte[] 1474560
$bootloader = [System.IO.File]::ReadAllBytes("build\bootloader.bin")
$kernel = [System.IO.File]::ReadAllBytes("build\kernel.bin")

[System.Array]::Copy($bootloader, 0, $floppy, 0, $bootloader.Length)
[System.Array]::Copy($kernel, 0, $floppy, 512, $kernel.Length)
[System.IO.File]::WriteAllBytes("build\os.flp", $floppy)

Write-Host ""
Write-Host "==========================================" -ForegroundColor Green
Write-Host "Build complete!" -ForegroundColor Green
Write-Host "==========================================" -ForegroundColor Green
Write-Host "Kernel size: $($kernel.Length) bytes"
Write-Host "Floppy image: build\os.flp"
Write-Host ""
Write-Host "Run with:" -ForegroundColor Yellow
Write-Host "  qemu-system-i386 -fda build\os.flp"