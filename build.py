#!/usr/bin/env python3
"""
CardboardOS - Unified Build System

Usage:
    python build.py              # Build everything
    python build.py --clean      # Clean and rebuild
    python build.py --iso        # Build ISO only (requires kernel)
    python build.py --run        # Build and run in QEMU
    python build.py --clean --run # Clean, build, and run
"""

import os
import sys
import shutil
import subprocess
import argparse
from pathlib import Path
from datetime import datetime
from typing import List, Dict, Optional, Tuple

# ============================================================================
# CONFIGURATION
# ============================================================================

class Config:
    """Build configuration"""
    
    # Toolchain
    TARGET = "x86_64-elf"
    CC = "clang"
    CXX = "clang++"
    ASM = "nasm"
    LD = "ld.lld"
    OBJCOPY = "llvm-objcopy"
    OBJDUMP = "llvm-objdump"
    SIZE = "llvm-size"
    
    # Compiler flags
    CFLAGS = [
        "-target", TARGET,
        "-ffreestanding",
        "-nostdlib",
        "-fno-stack-protector",
        "-fno-builtin",
        "-fno-exceptions",
        "-fno-rtti",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-O2",
        "-g",
        "-Iinclude",
    ]
    
    CXXFLAGS = CFLAGS + ["-std=c++17"]
    ASMFLAGS = ["-f", "elf64", "-F", "dwarf", "-g"]
    
    LDFLAGS = [
        "-target", TARGET,
        "-ffreestanding",
        "-nostdlib",
        "-fuse-ld=lld",
        "-Wl,--build-id=none",
        "-Wl,-z,max-page-size=0x1000",
        "-Wl,-z,common-page-size=0x1000",
        "-Wl,--gc-sections",
    ]
    
    # Paths
    ROOT = Path(__file__).parent.absolute()
    BUILD = ROOT / "build" / "out"
    OBJ_DIR = BUILD / "obj"
    KERNEL_DIR = BUILD / "kernel"
    ISO_DIR = BUILD / "iso"
    PROGRAMS_DIR = ISO_DIR / "programs"
    
    # Files
    KERNEL_ELF = KERNEL_DIR / "CardboardOS.elf"
    KERNEL_BIN = KERNEL_DIR / "CardboardOS.bin"
    ISO_FILE = BUILD / "CardboardOS.iso"
    USB_IMAGE = BUILD / "CardboardOS.img"
    
    # Source directories
    SRC_DIR = ROOT / "src" / "kernel"
    INCLUDE_DIR = ROOT / "include"
    PROGRAMS_SRC = ROOT / "programs"
    CONFIG_DIR = ROOT / "config"

# ============================================================================
# UTILITY FUNCTIONS
# ============================================================================

def print_header(text: str):
    """Print a section header"""
    print(f"\n{'='*70}")
    print(f"  {text}")
    print(f"{'='*70}")

def print_success(text: str):
    """Print success message"""
    print(f"[OK] {text}")

def print_error(text: str):
    """Print error message"""
    print(f"[ERROR] {text}")
    sys.exit(1)

def print_info(text: str):
    """Print info message"""
    print(f"[INFO] {text}")

def print_warning(text: str):
    """Print warning message"""
    print(f"[WARNING] {text}")

def print_step(text: str):
    """Print build step"""
    print(f"  -> {text}")

def run_command(cmd: List[str], description: str = "", cwd: Optional[Path] = None, 
                capture_output: bool = False, verbose: bool = True) -> Tuple[int, str, str]:
    """
    Run a shell command with error handling
    """
    if description:
        print_step(description)
    
    if verbose:
        print(f"    {' '.join(cmd)}")
    
    try:
        if capture_output:
            result = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True, check=False)
            return result.returncode, result.stdout, result.stderr
        else:
            result = subprocess.run(cmd, cwd=cwd, check=False)
            return result.returncode, "", ""
    except FileNotFoundError:
        print_error(f"Command not found: {cmd[0]}")
        return -1, "", ""
    except Exception as e:
        print_error(f"Failed to run command: {e}")
        return -1, "", ""

def clean_build():
    """Clean build directory"""
    print_header("Cleaning Build Directory")
    
    if Config.BUILD.exists():
        shutil.rmtree(Config.BUILD)
        print_success("Build directory cleaned")
    else:
        print_info("No build directory to clean")
    
    # Also clean any temporary files
    for pattern in ["*.tmp", "*.log", "*.cache"]:
        for f in Config.ROOT.rglob(pattern):
            f.unlink()
            print_step(f"Removed {f}")

# ============================================================================
# BUILD STAGES
# ============================================================================

class CardboardOSBuilder:
    """Main build orchestrator"""
    
    def __init__(self, verbose: bool = True):
        self.verbose = verbose
        self.object_files = []
        self.start_time = datetime.now()
        
        # Create build directories
        self._setup_dirs()
    
    def _setup_dirs(self):
        """Create all necessary build directories"""
        dirs = [
            Config.BUILD,
            Config.OBJ_DIR,
            Config.KERNEL_DIR,
            Config.ISO_DIR,
            Config.ISO_DIR / "boot" / "grub",
            Config.ISO_DIR / "kernel",
            Config.PROGRAMS_DIR,
        ]
        for d in dirs:
            d.mkdir(parents=True, exist_ok=True)
    
    def _get_source_files(self) -> Dict[str, List[Path]]:
        """Get all source files to compile"""
        src_dir = Config.SRC_DIR
        files = {
            'c': [],
            'cpp': [],
            'asm': [],
            's': [],
        }
        
        # C files
        for ext in ['*.c', '*.C']:
            files['c'].extend(src_dir.rglob(ext))
        
        # C++ files
        for ext in ['*.cpp', '*.cc', '*.cxx']:
            files['cpp'].extend(src_dir.rglob(ext))
        
        # NASM assembly
        files['asm'].extend(src_dir.rglob('*.asm'))
        
        # GNU assembly
        files['s'].extend(src_dir.rglob('*.s'))
        
        return files
    
    def compile_files(self):
        """Compile all source files"""
        print_header("Compiling Source Files")
        
        sources = self._get_source_files()
        all_files = sum(len(files) for files in sources.values())
        
        if all_files == 0:
            print_warning("No source files found!")
            return
        
        print_info(f"Found {all_files} source files")
        
        # Compile C files
        for c_file in sources['c']:
            self._compile_c_file(c_file)
        
        # Compile C++ files
        for cpp_file in sources['cpp']:
            self._compile_cpp_file(cpp_file)
        
        # Assemble NASM files
        for asm_file in sources['asm']:
            self._assemble_nasm(asm_file)
        
        # Assemble GNU AS files
        for s_file in sources['s']:
            self._assemble_gnu(s_file)
        
        if not self.object_files:
            print_error("No object files were created!")
        
        print_success(f"Compiled {len(self.object_files)} object files")
    
    def _compile_c_file(self, source: Path):
        """Compile a C file"""
        rel_path = source.relative_to(Config.SRC_DIR)
        obj_file = Config.OBJ_DIR / rel_path.with_suffix('.o')
        obj_file.parent.mkdir(parents=True, exist_ok=True)
        
        cmd = [
            Config.CC,
            *Config.CFLAGS,
            "-I", str(Config.SRC_DIR),
            "-c", str(source),
            "-o", str(obj_file),
        ]
        
        returncode, stdout, stderr = run_command(
            cmd, f"Compiling {rel_path}", capture_output=True,
            verbose=self.verbose
        )
        
        if returncode != 0:
            print_error(f"Failed to compile {source}\n{stderr}")
        
        self.object_files.append(obj_file)
    
    def _compile_cpp_file(self, source: Path):
        """Compile a C++ file"""
        rel_path = source.relative_to(Config.SRC_DIR)
        obj_file = Config.OBJ_DIR / rel_path.with_suffix('.o')
        obj_file.parent.mkdir(parents=True, exist_ok=True)
        
        cmd = [
            Config.CXX,
            *Config.CXXFLAGS,
            "-I", str(Config.SRC_DIR),
            "-c", str(source),
            "-o", str(obj_file),
        ]
        
        returncode, stdout, stderr = run_command(
            cmd, f"Compiling {rel_path}", capture_output=True,
            verbose=self.verbose
        )
        
        if returncode != 0:
            print_error(f"Failed to compile {source}\n{stderr}")
        
        self.object_files.append(obj_file)
    
    def _assemble_nasm(self, source: Path):
        """Assemble a NASM file with 64-bit support"""
        rel_path = source.relative_to(Config.SRC_DIR)
        obj_file = Config.OBJ_DIR / rel_path.with_suffix('.o')
        obj_file.parent.mkdir(parents=True, exist_ok=True)
        
        # Use elf64 format for 64-bit code
        cmd = [
            Config.ASM,
            "-f", "elf64",
            "-F", "dwarf",
            "-g",
            "-I", str(source.parent),
            str(source),
            "-o", str(obj_file),
        ]
        
        returncode, stdout, stderr = run_command(
            cmd, f"Assembling {rel_path}", capture_output=True,
            verbose=self.verbose
        )
        
        if returncode != 0:
            print_error(f"Failed to assemble {source}\n{stderr}")
        
        self.object_files.append(obj_file)
    
    def _assemble_gnu(self, source: Path):
        """Assemble a GNU assembly file"""
        rel_path = source.relative_to(Config.SRC_DIR)
        obj_file = Config.OBJ_DIR / rel_path.with_suffix('.o')
        obj_file.parent.mkdir(parents=True, exist_ok=True)
        
        cmd = [
            Config.CC,
            "-target", Config.TARGET,
            "-c", str(source),
            "-o", str(obj_file),
        ]
        
        returncode, stdout, stderr = run_command(
            cmd, f"Assembling {rel_path}", capture_output=True,
            verbose=self.verbose
        )
        
        if returncode != 0:
            print_error(f"Failed to assemble {source}\n{stderr}")
        
        self.object_files.append(obj_file)
    
    def link_kernel(self):
        """Link the kernel with ld.lld"""
        print_header("Linking Kernel")
        
        linker_script = Config.CONFIG_DIR / "linker.ld"
        if not linker_script.exists():
            print_error(f"Linker script not found: {linker_script}")
        
        # Link kernel
        cmd = [
            Config.CC,
            *Config.LDFLAGS,
            f"-Wl,-T,{linker_script}",
            "-o", str(Config.KERNEL_ELF),
        ] + [str(obj) for obj in self.object_files]
        
        returncode, stdout, stderr = run_command(
            cmd, "Linking kernel with ld.lld", capture_output=True,
            verbose=self.verbose
        )
        
        if returncode != 0:
            print_error(f"Failed to link kernel\n{stderr}")
        
        # Strip debug symbols
        cmd = [Config.OBJCOPY, "--strip-debug", str(Config.KERNEL_ELF)]
        run_command(cmd, "Stripping debug symbols", verbose=self.verbose)
        
        # Create raw binary
        cmd = [Config.OBJCOPY, "-O", "binary", str(Config.KERNEL_ELF), str(Config.KERNEL_BIN)]
        run_command(cmd, "Creating raw binary", verbose=self.verbose)
        
        # Show size information
        cmd = [Config.SIZE, str(Config.KERNEL_ELF)]
        run_command(cmd, "Kernel size", verbose=self.verbose)
        
        print_success(f"Kernel linked: {Config.KERNEL_ELF}")
        print_success(f"Binary: {Config.KERNEL_BIN}")
    
    def compile_notc_programs(self):
        """Compile NotC programs and copy to ISO"""
        print_header("Compiling NotC Programs")
        
        if not Config.PROGRAMS_SRC.exists():
            print_warning("No NotC programs found")
            return
        
        notc_files = list(Config.PROGRAMS_SRC.glob("*.notc"))
        if not notc_files:
            print_warning("No .notc files found")
            return
        
        print_info(f"Found {len(notc_files)} NotC programs")
        
        for notc_file in notc_files:
            self._compile_notc(notc_file)
        
        print_success(f"Compiled {len(notc_files)} NotC programs")
    
    def _compile_notc(self, source: Path):
        """Compile a NotC program (copy to ISO for now)"""
        print_step(f"Processing {source.name}")
        
        # Copy to ISO programs directory
        dest = Config.PROGRAMS_DIR / source.name
        shutil.copy2(source, dest)
        
        # Basic validation of NotC syntax
        self._validate_notc(source)
    
    def _validate_notc(self, source: Path):
        """Basic validation of NotC syntax"""
        try:
            content = source.read_text()
            
            # Check for basic structure
            if "main:" not in content:
                print_warning(f"{source.name}: No 'main:' label found")
            
            # Check for matching brackets
            open_brackets = content.count('[')
            close_brackets = content.count(']')
            if open_brackets != close_brackets:
                print_warning(f"{source.name}: Unbalanced brackets ({open_brackets} vs {close_brackets})")
            
        except Exception as e:
            print_warning(f"Failed to validate {source.name}: {e}")
    
    def create_iso(self):
        """Create bootable ISO with GRUB"""
        print_header("Creating Bootable ISO")
        
        # Copy kernel to ISO
        if not Config.KERNEL_ELF.exists():
            print_error("Kernel not found. Run build first.")
        
        shutil.copy2(Config.KERNEL_ELF, Config.ISO_DIR / "kernel" / "CardboardOS.elf")
        
        # Copy GRUB configuration
        grub_cfg = Config.CONFIG_DIR / "grub.cfg"
        if grub_cfg.exists():
            shutil.copy2(grub_cfg, Config.ISO_DIR / "boot" / "grub" / "grub.cfg")
        else:
            self._create_default_grub_cfg()
        
        # Create ISO using grub-mkrescue
        cmd = [
            "grub-mkrescue",
            "-o", str(Config.ISO_FILE),
            str(Config.ISO_DIR),
        ]
        
        returncode, stdout, stderr = run_command(
            cmd, "Creating ISO with grub-mkrescue", capture_output=True,
            verbose=self.verbose
        )
        
        if returncode != 0:
            print_warning("grub-mkrescue failed, trying xorriso...")
            self._create_iso_with_xorriso()
        else:
            print_success(f"ISO created: {Config.ISO_FILE}")
    
    def _create_default_grub_cfg(self):
        """Create a default GRUB configuration"""
        grub_dir = Config.ISO_DIR / "boot" / "grub"
        grub_dir.mkdir(parents=True, exist_ok=True)
        
        grub_cfg = grub_dir / "grub.cfg"
        content = """# CardboardOS GRUB Configuration
set timeout=5
set default=0

menuentry "CardboardOS" {
    multiboot /kernel/CardboardOS.elf
    boot
}

menuentry "CardboardOS (verbose)" {
    multiboot /kernel/CardboardOS.elf -v
    boot
}

menuentry "CardboardOS (safe mode)" {
    multiboot /kernel/CardboardOS.elf --safe
    boot
}
"""
        
        grub_cfg.write_text(content)
        print_step("Created default GRUB configuration")
    
    def _create_iso_with_xorriso(self):
        """Create ISO using xorriso directly"""
        cmd = [
            "xorriso",
            "-as", "mkisofs",
            "-b", "boot/grub/stage2_eltorito",
            "-no-emul-boot",
            "-boot-load-size", "4",
            "-boot-info-table",
            "--grub2-boot-info",
            "-V", "CardboardOS",
            "-o", str(Config.ISO_FILE),
            str(Config.ISO_DIR),
        ]
        
        returncode, stdout, stderr = run_command(
            cmd, "Creating ISO with xorriso", capture_output=True,
            verbose=self.verbose
        )
        
        if returncode != 0:
            print_error(f"Failed to create ISO\n{stderr}")
        
        print_success(f"ISO created: {Config.ISO_FILE}")
    
    def create_usb_image(self):
        """Create a bootable USB image with exFAT"""
        print_header("Creating USB Image")
        
        img_path = Config.USB_IMAGE
        
        # Create a 64MB image file
        cmd = ["dd", "if=/dev/zero", f"of={img_path}", "bs=1M", "count=64"]
        run_command(cmd, "Creating USB image file", verbose=self.verbose)
        
        # Format with exFAT
        if sys.platform == "win32":
            cmd = ["mkfs.exfat", str(img_path)]
        else:
            cmd = ["mkfs.exfat", "-n", "CARDBOARD", str(img_path)]
        
        returncode, stdout, stderr = run_command(
            cmd, "Formatting with exFAT", capture_output=True,
            verbose=self.verbose
        )
        
        if returncode != 0:
            print_warning("Failed to format USB image (tools may be missing)")
            print_info("You'll need to format the USB drive manually with exFAT")
        else:
            print_success(f"USB image created: {img_path}")
        
        print_info("Kernel must be copied to USB image separately")
        print_info("For testing, use the ISO or the kernel binary directly")

    def run_qemu(self):
        """Run the OS in QEMU"""
        print_header("Running in QEMU")
        
        if not Config.ISO_FILE.exists():
            print_warning("ISO not found. Building first...")
            self.build()
        
        # QEMU command
        cmd = [
            "qemu-system-x86_64",
            "-cdrom", str(Config.ISO_FILE),
            "-m", "256M",
            "-cpu", "qemu64",
            "-vga", "std",
            "-usb",
            "-device", "usb-mouse",
            "-device", "usb-kbd",
            "-rtc", "base=localtime",
            "-name", "CardboardOS",
        ]
        
        if self.debug:
            cmd.extend(["-s", "-S"])
        
        print_info("Starting QEMU...")
        print_info("Press Ctrl+Alt+G to release mouse")
        print_info("Press Ctrl+Alt+F to toggle fullscreen")
        
        try:
            subprocess.run(cmd, check=True)
        except KeyboardInterrupt:
            print_info("QEMU terminated by user")
        except Exception as e:
            print_error(f"Failed to run QEMU: {e}")
    
    def build(self):
        """Full build process"""
        print_header("CardboardOS Build System")
        print_info(f"Build started: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
        print_info(f"Configuration: {Config.TARGET}")
        
        self.compile_files()
        self.link_kernel()
        self.compile_notc_programs()
        self.create_iso()
        self.create_usb_image()
        
        self._show_summary()
        
        print_header("Build Complete")
        print_info(f"ISO: {Config.ISO_FILE}")
        print_info(f"Kernel: {Config.KERNEL_ELF}")
        print_info(f"Binary: {Config.KERNEL_BIN}")
        print_info(f"Build time: {datetime.now() - self.start_time}")
    
    def _show_summary(self):
        """Show build summary"""
        print_header("Build Summary")
        
        obj_count = len(self.object_files)
        print_info(f"Object files: {obj_count}")
        
        if Config.KERNEL_ELF.exists():
            size = Config.KERNEL_ELF.stat().st_size
            print_info(f"Kernel ELF: {size // 1024} KB")
        
        if Config.KERNEL_BIN.exists():
            size = Config.KERNEL_BIN.stat().st_size
            print_info(f"Kernel binary: {size // 1024} KB")
        
        if Config.ISO_FILE.exists():
            size = Config.ISO_FILE.stat().st_size
            print_info(f"ISO: {size // 1024} KB")
        
        notc_files = list(Config.PROGRAMS_DIR.glob("*.notc"))
        if notc_files:
            print_info(f"NotC programs: {len(notc_files)}")
            for f in notc_files:
                print_step(f"  {f.name}")

# ============================================================================
# MAIN ENTRY POINT
# ============================================================================

def main():
    """Main entry point with argument parsing"""
    parser = argparse.ArgumentParser(
        description="CardboardOS Build System",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python build.py               # Full build
  python build.py --clean       # Clean and build
  python build.py --run         # Build and run in QEMU
  python build.py --iso         # Build ISO only (requires kernel)
  python build.py --clean --run # Clean, build, and run
  python build.py --debug       # Build with debug symbols
        """
    )
    
    parser.add_argument("--clean", action="store_true",
                       help="Clean build directory before building")
    parser.add_argument("--iso", action="store_true",
                       help="Build ISO only (kernel must already be built)")
    parser.add_argument("--run", action="store_true",
                       help="Run in QEMU after building")
    parser.add_argument("--debug", action="store_true",
                       help="Enable debug symbols and QEMU debugging")
    parser.add_argument("--verbose", action="store_true",
                       help="Verbose output")
    
    args = parser.parse_args()
    
    builder = CardboardOSBuilder(verbose=args.verbose)
    builder.debug = args.debug
    
    if args.clean:
        clean_build()
        if len(sys.argv) == 2:
            return
    
    if args.iso:
        builder.create_iso()
        return
    
    builder.build()
    
    if args.run:
        builder.run_qemu()

if __name__ == "__main__":
    main()