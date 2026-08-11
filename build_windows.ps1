# build.ps1
Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "Building CardboardOS" -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan

# Create build directory
New-Item -ItemType Directory -Force -Path build | Out-Null

# Clean
Remove-Item -Force build\* -ErrorAction SilentlyContinue

# 1. Compile C kernel
Write-Host "[1/4] Compiling kmain.c..." -ForegroundColor Yellow
& clang -m32 -ffreestanding -nostdlib -fno-pie -fno-stack-protector -c src\kernel\kmain.c -o build\kernel.o -target i386-pc-none-elf
if ($LASTEXITCODE -ne 0) { exit 1 }

# 2. Link
Write-Host "[2/4] Linking kernel..." -ForegroundColor Yellow
& ld.lld -m elf_i386 -T src\linker.ld -o build\kernel.elf build\kernel.o
if ($LASTEXITCODE -ne 0) { exit 1 }

# 3. Extract binary
Write-Host "[3/4] Extracting kernel binary..." -ForegroundColor Yellow
& llvm-objcopy -O binary build\kernel.elf build\kernel.bin

# 4. Assemble bootloader
Write-Host "[4/4] Assembling bootloader..." -ForegroundColor Yellow
& nasm -f bin src\bootloader\boot.asm -o build\bootloader.bin
if ($LASTEXITCODE -ne 0) { exit 1 }

# 5. Create floppy image (1.44MB = 2880 sectors * 512 bytes = 1,474,560 bytes)
Write-Host "Creating floppy image..." -ForegroundColor Yellow
$floppy = New-Object byte[] 1474560
$bootloader = [System.IO.File]::ReadAllBytes("build\bootloader.bin")
$kernel = [System.IO.File]::ReadAllBytes("build\kernel.bin")

# Write bootloader to first sector (offset 0)
[System.Array]::Copy($bootloader, 0, $floppy, 0, $bootloader.Length)

# Write kernel starting at sector 2 (offset 512)
[System.Array]::Copy($kernel, 0, $floppy, 512, $kernel.Length)

# Save floppy image
[System.IO.File]::WriteAllBytes("build\floppy.img", $floppy)

$kernelSize = $kernel.Length
$floppySize = $floppy.Length

Write-Host ""
Write-Host "==========================================" -ForegroundColor Green
Write-Host "Build complete!" -ForegroundColor Green
Write-Host "==========================================" -ForegroundColor Green
Write-Host "Kernel size: $kernelSize bytes"
Write-Host "Floppy image: build\floppy.img ($floppySize bytes)"
Write-Host ""
Write-Host "Run with:" -ForegroundColor Yellow
Write-Host "  qemu-system-i386 -fda build\floppy.img"