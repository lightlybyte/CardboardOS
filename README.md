# CardboardOS

## An Operating System Built from Official Documentation Alone

CardboardOS is a one-year project to construct a functional x86-64 operating system from scratch, using no tutorials, no blog posts, no third-party guides, and no Stack Overflow. The only permitted references are the official manuals of the tools and architectures involved. Every line of assembly, every kernel routine, every build script is written by hand, informed solely by primary sources.

The project is a deliberate exercise in grounding, patience, and the willingness to read documentation until it yields meaning.

The name CardboardOS reflects the material from which it is built: not steel or silicon, but something assembled from thin layers, one at a time, fragile until it holds. It is an operating system constructed from the most basic possible materials: documentation, persistence, and a willingness to fail repeatedly.

---

## Motto

*"Tears of Intense Suffering, and Suffering, Even the devil may cry."*

---

## The 91.6 Percent Rule

The project was conceived during a near-total solar eclipse on 12 August 2026, with 91.6 percent of the sun obscured. The moment fixed itself as a metaphor for the work ahead: most of the knowledge required is darkness, and the visible portion is thin. The eclipse teaches that one need not see everything. One need only see the next step.

The same principle applies to the documentation. The Intel and AMD manuals total over 5,000 pages. The GCC and Clang documentation is similarly vast. Most of it is not needed at any given moment. The skill is not in reading everything. The skill is in knowing which three chapters to read.

---

## Motto of the Eclipse Update

*"Whoever fears the wrath of the sun shall be rendered weak upon my eyes. For the indomitable challenge the sun."*

---

## Goals

- Write a working bootloader in NASM that loads a kernel into memory.
- Enter protected mode, then long mode, using only the AMD and Intel manuals.
- Implement a minimal kernel in C++23 with freestanding compilation.
- Provide a simple memory allocator, interrupt handler, and keyboard driver.
- Build a functional userspace with a minimal shell.
- Document every step in a tutorial written from the perspective of someone who learned by reading official documentation alone.

The project is considered successful if, after one year, the operating system can boot, accept keyboard input, run a simple program, and shut down cleanly.

---

## Toolchain

The following tools are used throughout the project. All configuration and usage is derived from official documentation.

- GCC (cross-compiler for x86_64-elf)
- Clang / LLVM (for C++23 kernel code)
- NASM (bootloader assembly)
- GNU Binutils (linking, object manipulation)
- GNU Make (build automation)
- CMake (cross-platform build configuration)
- Ninja (fast incremental builds)
- QEMU (emulation and testing)
- GDB (debugging with QEMU's remote protocol)
- LLD (alternative linker, used where appropriate)
- ccache (speeds up recompilation)
- dos2unix (handles line endings across platforms)
- PExports (Windows-specific export handling)
- Doxygen (documentation generation for the kernel interface)

---

## Documentation Used

The following official documentation sources are referenced exclusively. No external tutorials, guides, or third-party explanations are permitted.

- AMD64 Architecture Programmer's Manual (Volumes 1-5)
- Intel 64 and IA-32 Architectures Software Developer's Manuals (Volumes 1-4)
- NASM Manual
- GNU GCC Manual (cross-compiler flags and freestanding options)
- GNU LD Manual (linker scripts)
- Clang / LLVM Documentation (C++23 freestanding compilation)
- QEMU Documentation (emulation and debugging)
- GDB Manual (remote debugging)
- System V Application Binary Interface (calling conventions)
- ELF Specification (executable format)
- C++23 Standard (language features used in the kernel)
- C Standard (minimal runtime used in the kernel)
- GNU Make Manual
- CMake Documentation
- PowerShell Documentation
- BASH Reference Manual
- BATCH Command Reference

---

## Build System

The project is designed to be built on multiple platforms. Three build scripts are maintained:

- build.sh — BASH script for Linux and macOS
- build.bat — BATCH script for Windows Command Prompt
- build.ps1 — PowerShell script for Windows

Each script invokes the appropriate cross-compiler, assembler, and linker to produce a bootable ISO image.

The build system is structured to support:

- Cross-compilation of the toolchain from source
- Assembly of the bootloader using NASM
- Compilation of the kernel using C++23 in freestanding mode
- Linking using a custom linker script
- Creation of a bootable ISO image using appropriate utilities

---
