# CardboardOS

CardboardOS is a hobbyist operating system for x86_64 that features:
- Graphical User Interface (GUI)
- exFAT filesystem support
- NotC programming language interpreter
- USB storage support
- Keyboard and mouse input

## Building

### Prerequisites
- Python 3.6+
- Clang/LLVM
- NASM
- GRUB2
- QEMU (optional, for testing)

### Build Commands
```bash
# Full build
python build.py

# Clean and rebuild
python build.py --clean

# Build and run in QEMU
python build.py --run

# Build with debug symbols
python build.py --debug --run