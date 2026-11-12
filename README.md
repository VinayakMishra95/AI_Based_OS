# AetherOS

A bare-metal 32-bit x86 (`i386`) operating system written from scratch in **C** and **NASM Assembly**, featuring a double-buffered 32-bit VBE GUI, ATA PIO disk persistence, round-robin task scheduling, and a COM1 UART **AI Serial Bridge**.

## Key Features
- **2-Stage Bootloader**: 512-byte MBR (`boot/boot.asm`) + Stage 2 (`boot/stage2.asm`) with Fast A20, `1024x768x32bpp` VBE framebuffer, and 32-bit Protected Mode.
- **Kernel & Drivers**: Flat GDT, 256-entry IDT with 8259 PIC remapping, 100 Hz PIT timer, PS/2 keyboard & mouse, COM1 UART (115200 baud), and 28-bit LBA ATA PIO disk driver.
- **Memory & Storage**: 16 MB coalescing kernel heap (`kmalloc`/`kfree` at `0x400000`) and persistent RAMFS synced to ATA LBA 200+.
- **Task Scheduler**: 8-slot round-robin task table (`ps`, `spawn`, `kill`).
- **32-bit GUI & AI Bridge**: Draggable **Terminal**, **SysMonitor**, and **AI Assistant** windows with a COM1 serial bridge (`tools/ai_bridge.py`) over TCP `127.0.0.1:4444`.

## Quick Start (macOS)
```bash
brew install qemu nasm lld
make run

Run with the AI Serial Bridge (in two terminals):
# Terminal 1
make run-bridge

# Terminal 2
make bridge

Shell Commands
ls, cat <file>, touch <file>, write <file> <text>, rm <file>, sync
ps, spawn <name>, kill <pid>
mem, uptime, uname, calc <a op b>, hexdump <0xADDR>, clear

