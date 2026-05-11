# DugOS

![Language: C](https://img.shields.io/badge/language-C%20%28freestanding%29-blue)
![Assembly: NASM](https://img.shields.io/badge/assembly-NASM%20x86-red)
![Architecture: i386](https://img.shields.io/badge/arch-x86%20i386%2032--bit-orange)
![Boot: GRUB 2](https://img.shields.io/badge/boot-GRUB%202%20Multiboot-brightgreen)
![Emulator: QEMU](https://img.shields.io/badge/emulator-QEMU-teal)
![Course: CMSC 125](https://img.shields.io/badge/course-CMSC%20125-purple)
![Status: In Development](https://img.shields.io/badge/status-in%20development-yellow)

A bare-metal 32-bit operating system built from scratch for **CMSC 125 — Operating System Concepts**. DugOS boots independently in a virtual machine, implements protected-mode CPU infrastructure, and is being developed phase-by-phase toward a full interactive shell with file and directory operations.

---

## Table of Contents

- [Project Overview](#project-overview)
- [Tech Stack](#tech-stack)
- [Project Structure](#project-structure)
- [Installation Guide](#installation-guide)
- [Development Workflow](#development-workflow)
- [Available Make Targets](#available-make-targets)
- [Common Issues & Fixes](#common-issues--fixes)
- [Contributor Onboarding](#contributor-onboarding)
- [Roadmap](#roadmap)
- [Credits & Team](#credits--team)
- [Repository Improvement Notes](#repository-improvement-notes)

---

## Project Overview

DugOS is a **from-scratch, freestanding operating system** written in C and x86 assembly. It does not run on top of an existing OS — it boots directly from GRUB inside QEMU (or any x86 virtual machine) and takes full control of the hardware.

The project studies the design of **MINIX 3.1.0** (specifically `kernel/main.c`, Appendix B, line 07100) and incorporates those concepts into a purpose-built kernel. Key design decisions mirror MINIX: a progressive boot sequence in `main.c`, explicit interrupt controller setup, a process table, and a microkernel-inspired structure.

### Key Features (current)

| Feature | Status |
|---|---|
| Boots independently in QEMU via GRUB 2 | ✅ Done |
| 32-bit protected mode | ✅ Done |
| VGA text-mode welcome screen | ✅ Done |
| Global Descriptor Table (GDT, 5 entries) | ✅ Done |
| Interrupt Descriptor Table (IDT, 256 entries) | ✅ Done |
| CPU exception handlers (vectors 0–31) | ✅ Done |
| PS/2 keyboard input | 🔄 Phase B.2–B.3 |
| Interactive shell (help, clear, shutdown) | 🔄 Phase B.4 |
| In-memory file system (read/write/edit/delete) | 🔄 Phase C |
| Directory operations (mkdir/cd/rmdir/ls) | 🔄 Phase D |
| PIC remap and hardware IRQ handling | 🔄 Phase B.2 |

### Target Users

- **Course instructors / graders** evaluating CMSC 125 deliverables
- **Team developers** working on the OS phases
- **Future maintainers** extending the kernel
- **Recruiters / portfolio reviewers** assessing systems programming experience

---

## Tech Stack

| Category | Tool / Technology |
|---|---|
| **Primary language** | C (freestanding, no libc) |
| **Assembly** | NASM (Intel syntax, 32-bit ELF) |
| **Linker** | GNU `ld` (`-m elf_i386`) |
| **Compiler** | `gcc -m32 -ffreestanding -nostdlib` (via `gcc-multilib`) |
| **Build system** | GNU Make |
| **Bootloader** | GRUB 2 (Multiboot 1 specification) |
| **ISO creation** | `grub-mkrescue` + `xorriso` + `mtools` |
| **Emulator** | `qemu-system-i386` |
| **Host environment** | WSL2 + Ubuntu (required for Linux toolchain on Windows) |
| **Reference OS** | MINIX 3.1.0 (read-only, in `reference/minix/`) |

> **No standard library.** DugOS uses only hardware registers, inline assembly, and its own VGA driver. There is no `printf`, `malloc`, or any POSIX function in the kernel.

---

## Project Structure

```
DugOS/
│
├── src/                        # Kernel source — the actual OS
│   ├── main.c                  # Kernel entry point (kmain). Mirrors MINIX
│   │                           #   main.c line 07100. Boot stages are
│   │                           #   uncommented progressively each phase.
│   ├── boot.s                  # NASM: Multiboot 1 header + _start entry.
│   │                           #   Sets up 16 KiB stack, calls kmain.
│   ├── vga.h / vga.c           # VGA text-mode driver (80×25, 0xB8000).
│   │                           #   Provides vga_write, vga_writeln,
│   │                           #   vga_set_color, vga_write_hex/dec.
│   ├── gdt.h / gdt.c           # Global Descriptor Table setup (5 entries:
│   │                           #   null, kernel code/data, user code/data).
│   ├── idt.h / idt.c           # Interrupt Descriptor Table (256 entries).
│   │                           #   Wires ISR stubs for exceptions 0–31.
│   ├── isr.h / isr.c           # Common C exception handler. Prints vector
│   │                           #   number + name + error code, halts.
│   ├── isr_stubs.s             # NASM: Per-vector ISR stubs (ISR_NOERR /
│   │                           #   ISR_ERR macros). Also contains
│   │                           #   gdt_flush and idt_flush helpers.
│   ├── linker.ld               # Linker script. Loads kernel at 1 MiB.
│   │                           #   Section order: multiboot → text →
│   │                           #   rodata → data → bss.
│   ├── grub.cfg                # GRUB 2 menu entry. Boots dugos.elf via
│   │                           #   the multiboot protocol.
│   └── Makefile                # Build system. Targets: all, iso, run, clean.
│
├── reference/
│   └── minix/                  # MINIX 3.1.0 source (READ-ONLY reference).
│       ├── kernel/             #   Especially kernel/main.c, protect.c,
│       ├── drivers/            #   i8259.c, clock.c for design patterns.
│       ├── servers/            #   Do not modify or build from this tree.
│       └── include/
│
├── dug_os.c                    # Windows userland prototype. Simulates the
│                               #   boot sequence and shell using stdio.
│                               #   Use as behavior reference for Phase B–D
│                               #   (commands, prompts, file ops).
│
├── setup.sh                    # One-shot WSL2/Ubuntu toolchain bootstrap.
│                               #   Run once on a fresh install.
├── .gitignore                  # Excludes *.o, *.elf, *.iso, src/iso/,
│                               #   *.exe, .claude/, .vscode/.
├── .gitattributes              # Forces LF line endings on all source files
│                               #   so Makefile and shell scripts work on
│                               #   any platform.
└── README.md                   # This file.
```

---

## Installation Guide

### Prerequisites

DugOS must be built on a **Linux environment**. On Windows, use **WSL2 with Ubuntu** (recommended). Native Linux or macOS with the right toolchain also works.

### Step 1 — Clone the repository

```bash
git clone https://github.com/aaaranas/DugOS.git
cd DugOS
```

### Step 2 — Set up WSL2 (Windows only)

Open **PowerShell as Administrator** and run:

```powershell
wsl --install -d Ubuntu
```

Reboot when prompted. On first Ubuntu launch, create a username and password. Then access the repo from WSL:

```bash
cd /mnt/c/Users/<YourUser>/Desktop/DugOS
```

### Step 3 — Install the toolchain

Run the bootstrap script **once** on a fresh Ubuntu install:

```bash
bash setup.sh
```

This installs:

| Package | Purpose |
|---|---|
| `build-essential` | gcc, make, standard tools |
| `gcc-multilib` | 32-bit cross-compilation (`-m32`) |
| `nasm` | NASM assembler for `boot.s` and `isr_stubs.s` |
| `qemu-system-x86` | x86 emulator (`qemu-system-i386`) |
| `grub-pc-bin` | GRUB 2 binary modules |
| `grub-common` | `grub-mkrescue` tool |
| `xorriso` | ISO 9660 image creation |
| `mtools` | Required by `grub-mkrescue` |

### Step 4 — Build and run

```bash
cd src
make run
```

A QEMU window opens. You should see:
1. The yellow DugOS welcome banner
2. `>> GDT loaded (5 entries)` (green)
3. `>> IDT loaded (256 entries, 32 exception handlers)` (green)
4. `>> DugOS booted. CPU halted until Phase B implements input.` (white)

### Step 5 — (Optional) Test exception handling

In `src/main.c`, uncomment line 33:

```c
__asm__ __volatile__ ("int $3");
```

Run `make run`. A red `!! EXCEPTION 3 (Breakpoint)` message confirms the IDT is correctly wired. **Re-comment the line afterward.**

---

## Environment Variables

DugOS is a bare-metal kernel — it has no runtime environment and therefore no `.env` file. There are no environment variables to configure.

**QEMU launch options** are the closest equivalent. The default `make run` uses:

```makefile
qemu-system-i386 -cdrom dugos.iso -boot d
```

For debugging, you can pass additional flags manually:

```bash
# Log CPU resets and interrupts to a file (useful for triple-fault debugging)
qemu-system-i386 -cdrom src/dugos.iso -boot d -d int,cpu_reset -no-reboot 2>qemu.log
```

---

## Development Workflow

### Branch naming

| Prefix | Purpose | Example |
|---|---|---|
| `vm/<name>` | VM/build toolchain work | `vm/dom` |
| `feature/<name>` | New OS feature | `feature/keyboard-driver` |
| `fix/<name>` | Bug fix | `fix/idt-selector` |
| `phase/<letter>` | Phase implementation | `phase/b2-pic` |

### Commit conventions

Commits follow a **phase-prefixed** style:

```
Phase B.1: GDT and IDT scaffolding

Short body explaining what changed and why. Focus on the "why",
not just the "what". Reference MINIX source files where relevant
(e.g., mirrors MINIX kernel/protect.c).
```

- Subject line: `Phase X.Y: short imperative description`
- Body: optional, explains design decisions
- No trailing co-author lines

### Phased development rule

**`src/main.c` is the conductor.** Boot stages are added as commented-out stubs and uncommented one at a time, matching the spec's "implement first, comment out rest" requirement. Every phase ends with a working `make run` before the next begins.

```
Phase A  → Phase B.1 → Phase B.2 → Phase B.3 → Phase B.4
  ↓                                                ↓
Boot +        GDT +       PIC       Keyboard     Shell
Welcome       IDT       remap      driver
```

### Pull request process

1. Branch from `main`
2. Implement one slice (never two phases in one PR)
3. Confirm `make run` shows the expected output
4. Open PR with description of what changed and a screenshot of QEMU output
5. Team review before merge to `main`

---

## Available Make Targets

All targets must be run from inside the `src/` directory.

```bash
cd src
```

| Target | Command | Description |
|---|---|---|
| **Build kernel** | `make` or `make all` | Compiles all `.c` and `.s` files, links `dugos.elf` |
| **Build ISO** | `make iso` | Creates `dugos.iso` bootable with GRUB 2 |
| **Run in QEMU** | `make run` | Builds ISO and launches `qemu-system-i386` |
| **Clean** | `make clean` | Removes all `.o`, `.elf`, `.iso`, and the `iso/` directory |

```bash
# Full clean build and boot
make clean && make run

# Build only (no QEMU)
make all

# Inspect the compiled ELF
readelf -h dugos.elf
objdump -d dugos.elf | less
```

---

## Common Issues & Fixes

### `make: gcc: Command not found` or `-m32` errors

**Cause:** `gcc-multilib` is not installed.

```bash
sudo apt install gcc-multilib
```

### `grub-mkrescue: command not found`

**Cause:** GRUB tools or ISO utilities missing.

```bash
sudo apt install grub-pc-bin grub-common xorriso mtools
```

### `qemu-system-i386: command not found`

**Cause:** QEMU not installed.

```bash
sudo apt install qemu-system-x86
```

### QEMU window does not open (WSL, no display)

**Cause:** WSLg (WSL2 GUI support) is not enabled or the X server isn't running.

- Ensure you are on **WSL2** (not WSL1): `wsl --list --verbose`
- Update WSL: `wsl --update` (requires Windows 11 or Windows 10 with update KB5004296)
- As a fallback, add `-nographic -serial stdio` to the QEMU command in the Makefile to get serial output in the terminal.

### QEMU reboots in a loop (triple fault)

**Cause:** A CPU exception occurred before the IDT was loaded, or a bad GDT descriptor caused a GPF on `gdt_flush`.

```bash
# Capture interrupt log
qemu-system-i386 -cdrom dugos.iso -boot d -d int,cpu_reset -no-reboot 2>qemu.log
cat qemu.log | head -50
```

Look for `check_exception` entries to identify the faulting vector.

### `Makefile: *** missing separator` error

**Cause:** Makefile recipe lines use spaces instead of tabs. This happens when the file is opened in an editor that converts tabs.

```bash
# Check for tab characters in the Makefile
cat -A src/Makefile | grep -P "^\s" | head -5
```

Ensure your editor is configured to preserve tabs in Makefiles. The `.gitattributes` enforces LF, but tab handling is editor-dependent.

### Line ending issues (`\r` in shell scripts)

**Cause:** Git on Windows converted LF to CRLF despite `.gitattributes`.

```bash
# Fix in-place
sed -i 's/\r//' setup.sh
sed -i 's/\r//' src/Makefile
```

### Build artifact name collision (`.c` and `.s` with same base name)

**Cause:** A `.c` file and a `.s` file sharing the same base name both produce `foo.o`. The Makefile treats them as a single object and links it twice.

**Fix:** Always use distinct base names. Example: `isr.c` (handler) + `isr_stubs.s` (assembly stubs) — not `isr.c` + `isr.s`.

---

## Contributor Onboarding

### Where to start

1. Read `src/main.c` — it is the single source of truth for boot order. Every phase adds a new live function call here.
2. Read the phase you are implementing against the MINIX reference:
   - **B.2 (PIC remap):** `reference/minix/kernel/i8259.c`
   - **B.3 (keyboard):** `reference/minix/drivers/tty/keyboard.c`
   - **B.4+ (shell/FS):** `dug_os.c` for behavioral reference (command names, prompts, error messages)
3. Run `make run` to confirm you have a working baseline before touching any file.

### Files and folders to avoid modifying

| Path | Reason |
|---|---|
| `reference/minix/` | Read-only MINIX 3.1.0 source. Study only, never modify or build. |
| `src/linker.ld` | Changing load address or section order can silently break the multiboot contract with GRUB. Discuss before touching. |
| `src/boot.s` | The CPU starts here. Any mistake causes an immediate triple fault with no diagnostic. |
| `src/isr_stubs.s` | The struct layout in `isr.h` is coupled to the push order in this file. Both must change together. |

### Coding conventions

- **No standard library.** No `#include <stdio.h>`, `malloc`, `printf`, or POSIX functions in the kernel. Everything goes through `vga_write*` or registers.
- **C standard:** `gnu99` (see Makefile `CFLAGS`).
- **Comments:** Heavy commenting is **required by the project spec** (Req 6) and counts toward the 70% correctness grade. Every function needs a header block explaining its purpose, parameters, and key behavior. See `src/vga.c` or `src/gdt.c` for the established style.
- **Naming:** `snake_case` for all C identifiers. Prefix by module (`vga_`, `gdt_`, `idt_`, `isr_`).
- **Assembly:** NASM Intel syntax only. AT&T syntax (used by GNU `as`) is not used.
- **One feature per commit.** Don't bundle an unrelated cleanup into a feature commit.

### Adding a new subsystem

1. Create `src/<subsystem>.h` (public interface) and `src/<subsystem>.c` (implementation).
2. If assembly is needed: `src/<subsystem>_stubs.s` (not `<subsystem>.s` — avoids the name collision bug).
3. Add both to `C_SRCS` / `ASM_SRCS` in `src/Makefile` and add a per-object header dependency.
4. Wire the init call into `kmain()` in `src/main.c`, following the existing pattern.
5. `make clean && make run` must pass before committing.

### Architecture notes

- **Memory model:** Flat 4 GiB address space, ring 0 only (for now). GDT has user-mode segments (ring 3) stubbed for future use.
- **VGA buffer:** Physical address `0xB8000`, 80×25 cells of 16-bit entries (attribute byte high, ASCII low).
- **Exception vs IRQ:** Exceptions are vectors 0–31 (handled in `isr_stubs.s` / `isr.c`). Hardware IRQs are vectors 32–47 (to be wired in Phase B.2 via 8259A PIC remap).
- **No memory allocator yet:** All kernel data structures are statically allocated (BSS or `.data` section). Phase C will need a simple bump allocator for FS nodes.
- **Struct layout coupling:** `struct regs` in `isr.h` must exactly match the push order in `isr_stubs.s:isr_common`. If you change one, change the other.

---

## Roadmap

| Phase | Milestone | Status |
|---|---|---|
| **A** | Bootable kernel, VGA welcome screen, infinite halt loop | ✅ Complete |
| **B.1** | GDT (5 entries) + IDT (256 entries, exception handlers 0–31) | ✅ Complete |
| **B.2** | 8259A PIC remap (`intr_init`), IRQ0 timer tick visible on screen | 🔄 Planned |
| **B.3** | PS/2 keyboard driver, IRQ1 handler, scan-code → ASCII, echo to screen | 🔄 Planned |
| **B.4** | Line-buffered shell: `help`, `clear`, `shutdown` | 🔄 Planned |
| **C** | FAT file system (linked allocation, 32 KB blocks): `fwrite`, `fread`, `fedit`, `fdel` | 🔄 Planned |
| **D** | Directory operations: `mkdir`, `cd`, `rmdir`, `ls` | 🔄 Planned |
| **E** | Aesthetics pass, creativity extras (color themes, ASCII art, echo/whoami/date) | 🔄 Planned |

> The `dug_os.c` prototype in the repo root contains fully working implementations of Phases B.4–D in user-space C. Use it as the behavioral specification when porting each command to the freestanding kernel.

---

## Credits & Team

| Role | Name |
|---|---|
| OS Development / VM & Build | Dominique |
| OS Development | *(teammate — add name)* |
| OS Development | *(teammate — add name)* |
| OS Development | *(teammate — add name)* |

**Course:** CMSC 125 — Operating System Concepts

**Reference implementation:** [MINIX 3.1.0](https://www.minix3.org/) by Andrew S. Tanenbaum and the Vrije Universiteit Amsterdam. The vendored source in `reference/minix/` is used for study purposes only.

---

## Repository Improvement Notes

### Suggested structural improvements

1. **Add a top-level `Makefile`** that forwards to `src/Makefile`, so contributors can run `make run` from the repo root instead of `cd src && make run`.
2. **Move `dug_os.c` and `dug_os.exe`** into `reference/prototype/` to cleanly separate reference material from the real kernel.
3. **Add `src/include/`** as a dedicated header directory when the number of headers grows past 8–10 files.
4. **Tag releases** at the end of each completed phase (`git tag phase-A`, `git tag phase-B1`, etc.) so graders and reviewers can check out specific milestones.

### Missing documentation to add later

| Document | Priority | Notes |
|---|---|---|
| `ARCHITECTURE.md` | High | Explain memory map, boot sequence, GDT/IDT layout, planned FS structure |
| `CONTRIBUTING.md` | Medium | PR checklist, "how to add a new syscall/command", issue reporting |
| `CHANGELOG.md` | Medium | Track what changed each phase; useful for the oral defense |
| `SECURITY.md` | Low | Scope of the project (no network, single user, no privilege separation yet) |
| `docs/minix-reference.md` | Medium | Notes on which MINIX files map to which DugOS source files |

### Suggested additional files

```
CONTRIBUTING.md   — Contributor guide (PR checklist, coding standards)
ARCHITECTURE.md   — Memory layout diagram, boot sequence, subsystem map
CHANGELOG.md      — Per-phase changelog (mirrors the roadmap above)
SECURITY.md       — Security scope disclaimer for this educational OS
```

---

*This README was last updated at Phase B.1 + comment retrofit (commit `c7a3999`). Update the feature table and roadmap status as each phase is completed.*
