#!/usr/bin/env python3
"""
Cross-platform build script for bootloader with GRUB 0.95
Source files in src/, stage2_eltorito in root
Works on Linux and Windows (with ld.lld)
"""

import os
import sys
import subprocess
import shutil
import platform
from pathlib import Path

class Colors:
    """ANSI color codes for terminal output"""
    RED = '\033[91m'
    GREEN = '\033[92m'
    YELLOW = '\033[93m'
    BLUE = '\033[94m'
    MAGENTA = '\033[95m'
    CYAN = '\033[96m'
    RESET = '\033[0m'
    BOLD = '\033[1m'

def print_color(text, color=Colors.GREEN):
    """Print colored text if terminal supports it"""
    if sys.stdout.isatty() and platform.system() != 'Windows':
        print(f"{color}{text}{Colors.RESET}")
    else:
        print(text)

def run_command(cmd, cwd=None, capture_output=False):
    """Run a command and handle errors"""
    print_color(f"  Running: {' '.join(cmd)}", Colors.CYAN)
    
    try:
        if capture_output:
            result = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True, check=False)
            return result
        else:
            subprocess.run(cmd, cwd=cwd, check=True)
            return None
    except subprocess.CalledProcessError as e:
        print_color(f"  Command failed with exit code {e.returncode}", Colors.RED)
        if e.stderr:
            print(e.stderr)
        sys.exit(1)
    except FileNotFoundError:
        print_color(f"  Command not found: {cmd[0]}", Colors.RED)
        sys.exit(1)

def detect_toolchain():
    """Detect available toolchain based on platform"""
    system = platform.system()
    is_windows = system == 'Windows'
    
    # Detect assembler
    nasm_path = shutil.which('nasm')
    if not nasm_path:
        print_color("Error: nasm not found. Please install NASM.", Colors.RED)
        sys.exit(1)
    
    # Detect C compiler
    if is_windows:
        # Try clang first (usually comes with lld)
        cc = shutil.which('clang')
        if not cc:
            # Try gcc
            cc = shutil.which('gcc')
    else:
        cc = shutil.which('gcc')
        if not cc:
            cc = shutil.which('clang')
    
    if not cc:
        print_color("Error: No C compiler found (gcc or clang)", Colors.RED)
        sys.exit(1)
    
    # Detect linker
    if is_windows:
        # Try ld.lld first
        linker = shutil.which('ld.lld')
        if not linker:
            # Try lld-link
            linker = shutil.which('lld-link')
        if not linker:
            # Try GNU ld
            linker = shutil.which('ld')
    else:
        linker = shutil.which('ld')
        if not linker:
            linker = shutil.which('ld.lld')
    
    if not linker:
        print_color("Error: No linker found (ld, ld.lld, or lld-link)", Colors.RED)
        sys.exit(1)
    
    return {
        'nasm': nasm_path,
        'cc': cc,
        'linker': linker,
        'is_windows': is_windows,
        'system': system
    }

def build_kernel(toolchain, src_dir, output_dir):
    """Build the kernel binary"""
    print_color(f"\n{BOLD}Building kernel...{Colors.RESET}", Colors.BLUE)
    
    # Create output directory
    output_dir.mkdir(exist_ok=True)
    
    # Build boot.asm from src/
    print_color("  Assembling src/boot.asm...", Colors.YELLOW)
    boot_obj = output_dir / 'boot.o'
    run_command([
        toolchain['nasm'],
        '-f', 'elf32',
        '-o', str(boot_obj),
        str(src_dir / 'boot.asm')
    ])
    
    # Build kmain.c from src/
    print_color("  Compiling src/kmain.c...", Colors.YELLOW)
    kmain_obj = output_dir / 'kmain.o'
    cc_cmd = [
        toolchain['cc'],
        '-m32',
        '-ffreestanding',
        '-fno-pie',
        '-nostdlib',
        '-c',
        str(src_dir / 'kmain.c'),
        '-o', str(kmain_obj)
    ]
    
    # Add Windows-specific flags
    if toolchain['is_windows']:
        # For Windows, add these flags for compatibility
        cc_cmd.extend(['-target', 'i386-pc-windows-msvc'])
        # Remove -fno-pie if clang doesn't support it
        if 'clang' in toolchain['cc']:
            cc_cmd.remove('-fno-pie')
    
    run_command(cc_cmd)
    
    # Link kernel
    print_color("  Linking kernel...", Colors.YELLOW)
    kernel_bin = output_dir / 'kernel.bin'
    ld_cmd = [
        toolchain['linker'],
        '-m', 'elf_i386',
        '-Ttext', '0x100000',
        '-e', 'start',
        '-o', str(kernel_bin),
        str(boot_obj),
        str(kmain_obj)
    ]
    
    # Handle different linkers
    if 'ld.lld' in toolchain['linker'] or 'lld-link' in toolchain['linker']:
        # lld uses different flags
        ld_cmd = [
            toolchain['linker'],
            '-m', 'elf_i386',
            '-Ttext', '0x100000',
            '--entry', 'start',
            '-o', str(kernel_bin),
            str(boot_obj),
            str(kmain_obj)
        ]
        # Remove incompatible flags
        if '-e' in ld_cmd:
            idx = ld_cmd.index('-e')
            ld_cmd.pop(idx)
            ld_cmd.pop(idx)
    
    # For Windows, use -mi386pe if available
    if toolchain['is_windows'] and 'ld.lld' not in toolchain['linker']:
        try:
            ld_cmd[ld_cmd.index('-m') + 1] = 'elf_i386'
        except (ValueError, IndexError):
            pass
    
    run_command(ld_cmd)
    
    # Check if kernel was created
    if not kernel_bin.exists():
        print_color("Error: Kernel binary not created!", Colors.RED)
        sys.exit(1)
    
    kernel_size = kernel_bin.stat().st_size
    print_color(f"  Kernel size: {kernel_size} bytes", Colors.GREEN)
    
    return kernel_bin

def create_iso(toolchain, kernel_bin, output_dir, stage2_path):
    """Create bootable ISO with GRUB 0.95"""
    print_color(f"\n{BOLD}Creating ISO image...{Colors.RESET}", Colors.BLUE)
    
    # Create ISO directory structure
    iso_dir = output_dir / 'iso'
    boot_dir = iso_dir / 'boot'
    grub_dir = boot_dir / 'grub'
    
    print_color("  Creating directory structure...", Colors.YELLOW)
    grub_dir.mkdir(parents=True, exist_ok=True)
    
    # Copy kernel
    print_color("  Copying kernel...", Colors.YELLOW)
    shutil.copy(kernel_bin, boot_dir / 'kernel.bin')
    
    # Copy stage2_eltorito if provided and exists
    if stage2_path and stage2_path.exists():
        print_color(f"  Using stage2_eltorito: {stage2_path}", Colors.YELLOW)
        shutil.copy(stage2_path, boot_dir / 'stage2_eltorito')
        has_stage2 = True
    else:
        print_color("  Warning: stage2_eltorito not found in root!", Colors.YELLOW)
        print_color("  Will try to use system GRUB...", Colors.YELLOW)
        has_stage2 = False
    
    # Create GRUB config for GRUB 0.95
    print_color("  Creating GRUB config...", Colors.YELLOW)
    grub_cfg = grub_dir / 'menu.lst'  # GRUB 0.95 uses menu.lst
    with open(grub_cfg, 'w') as f:
        f.write("""
# GRUB 0.95 config file
timeout 5
default 0

title My C Kernel
    kernel /boot/kernel.bin
    boot
""")
    
    # Also create grub.cfg for compatibility with newer GRUB
    with open(grub_dir / 'grub.cfg', 'w') as f:
        f.write("""
set timeout=5
set default=0

menuentry "My C Kernel" {
    multiboot /boot/kernel.bin
    boot
}
""")
    
    # Create ISO using mkisofs or genisoimage
    print_color("  Creating ISO image...", Colors.YELLOW)
    iso_path = output_dir / 'boot.iso'
    
    # Try different ISO creation tools
    mkisofs_tools = ['mkisofs', 'genisoimage', 'xorriso']
    mkisofs_found = False
    
    for tool in mkisofs_tools:
        if shutil.which(tool):
            mkisofs_found = True
            print_color(f"  Using {tool}...", Colors.YELLOW)
            
            if tool == 'xorriso':
                # xorriso command
                mkisofs_cmd = [
                    'xorriso',
                    '-as', 'mkisofs',
                    '-R',
                    '-no-emul-boot',
                    '-boot-load-size', '4',
                    '-boot-info-table',
                ]
                
                if has_stage2:
                    mkisofs_cmd.extend(['-b', 'boot/grub/stage2_eltorito'])
                else:
                    # Try to use system boot image
                    mkisofs_cmd.extend(['-b', 'boot/grub/stage2_eltorito'])
                
                mkisofs_cmd.extend([
                    '-o', str(iso_path),
                    str(iso_dir)
                ])
            else:
                # mkisofs/genisoimage command
                mkisofs_cmd = [
                    tool,
                    '-R',
                    '-no-emul-boot',
                    '-boot-load-size', '4',
                    '-boot-info-table',
                ]
                
                if has_stage2:
                    mkisofs_cmd.extend(['-b', 'boot/grub/stage2_eltorito'])
                
                mkisofs_cmd.extend([
                    '-o', str(iso_path),
                    str(iso_dir)
                ])
            
            run_command(mkisofs_cmd)
            break
    
    if not mkisofs_found:
        print_color("  Warning: mkisofs/genisoimage/xorriso not found!", Colors.YELLOW)
        print_color("  Creating raw floppy image instead...", Colors.YELLOW)
        
        # Create floppy image as fallback
        floppy_path = output_dir / 'floppy.img'
        with open(floppy_path, 'wb') as f:
            # Create 1.44MB floppy image
            f.write(b'\x00' * 1474560)
        
        print_color(f"  Floppy image created: {floppy_path}", Colors.GREEN)
        print_color("  Note: Use qemu-system-i386 -fda floppy.img", Colors.YELLOW)
        return floppy_path
    
    # Verify ISO was created
    if not iso_path.exists():
        print_color("Error: ISO not created!", Colors.RED)
        sys.exit(1)
    
    iso_size = iso_path.stat().st_size
    print_color(f"  ISO size: {iso_size} bytes", Colors.GREEN)
    
    return iso_path

def main():
    """Main build function"""
    print_color(f"{BOLD}=== Bootloader Build System ==={Colors.RESET}", Colors.MAGENTA)
    print_color(f"Platform: {platform.system()} {platform.release()}", Colors.CYAN)
    
    # Setup directories
    root_dir = Path(__file__).parent.absolute()
    src_dir = root_dir / 'src'
    output_dir = root_dir / 'build'
    stage2_path = root_dir / 'stage2_eltorito'
    
    # Check if src directory exists
    if not src_dir.exists():
        print_color(f"Error: src/ directory not found at {src_dir}", Colors.RED)
        sys.exit(1)
    
    # Check if source files exist
    if not (src_dir / 'boot.asm').exists():
        print_color(f"Error: src/boot.asm not found!", Colors.RED)
        sys.exit(1)
    if not (src_dir / 'kmain.c').exists():
        print_color(f"Error: src/kmain.c not found!", Colors.RED)
        sys.exit(1)
    
    # Parse arguments
    import argparse
    parser = argparse.ArgumentParser(description='Build bootloader with GRUB 0.95')
    parser.add_argument('--clean', action='store_true', help='Clean build directory')
    parser.add_argument('--run', action='store_true', help='Run in QEMU after build')
    parser.add_argument('--no-stage2', action='store_true', help='Ignore stage2_eltorito file')
    args = parser.parse_args()
    
    # Clean if requested
    if args.clean and output_dir.exists():
        print_color(f"Cleaning {output_dir}...", Colors.YELLOW)
        shutil.rmtree(output_dir)
    
    output_dir.mkdir(exist_ok=True)
    
    # Check for stage2_eltorito
    if not args.no_stage2 and stage2_path.exists():
        print_color(f"Found stage2_eltorito: {stage2_path}", Colors.GREEN)
    else:
        if args.no_stage2:
            print_color("Ignoring stage2_eltorito (--no-stage2 flag)", Colors.YELLOW)
        else:
            print_color("stage2_eltorito not found in root directory", Colors.YELLOW)
            print_color("Will try to use system GRUB", Colors.YELLOW)
    
    # Detect toolchain
    toolchain = detect_toolchain()
    print_color(f"  NASM: {toolchain['nasm']}", Colors.CYAN)
    print_color(f"  CC: {toolchain['cc']}", Colors.CYAN)
    print_color(f"  Linker: {toolchain['linker']}", Colors.CYAN)
    
    # Build kernel
    kernel_bin = build_kernel(toolchain, src_dir, output_dir)
    
    # Create ISO
    stage2_to_use = None if args.no_stage2 else stage2_path
    iso_path = create_iso(toolchain, kernel_bin, output_dir, stage2_to_use)
    
    # Final success message
    print_color(f"\n{BOLD}{Colors.GREEN}Build successful!{Colors.RESET}", Colors.GREEN)
    print_color(f"Output: {iso_path}", Colors.GREEN)
    
    # Print usage instructions
    print_color(f"\n{BOLD}To run in QEMU:{Colors.RESET}", Colors.CYAN)
    print(f"  qemu-system-i386 -cdrom {iso_path}")
    if iso_path.suffix == '.img':
        print(f"  qemu-system-i386 -fda {iso_path}")
    print()
    
    # Run in QEMU if requested
    if args.run:
        print_color(f"\n{BOLD}Running in QEMU...{Colors.RESET}", Colors.BLUE)
        qemu_cmd = ['qemu-system-i386', '-cdrom', str(iso_path)]
        if iso_path.suffix == '.img':
            qemu_cmd = ['qemu-system-i386', '-fda', str(iso_path)]
        
        if shutil.which('qemu-system-i386'):
            run_command(qemu_cmd)
        else:
            print_color("Error: qemu-system-i386 not found!", Colors.RED)
            print_color("Install QEMU or run manually with the command above", Colors.YELLOW)
            sys.exit(1)

if __name__ == '__main__':
    main()