# DugOS — Task Handoff Document

**Prepared by:** Dominique (VM/Build lead, `vm/dom` branch)
**Handoff date:** 2026-05-11
**Current branch:** `vm/dom`
**Repo:** https://github.com/aaaranas/DugOS
**Course:** CMSC 125 — Operating System Concepts

> This document is for teammates picking up OS development after Phase B.1. Read it fully before touching any source file.

---

## Current Project Status

### Completed phases

| Phase | Description | Commit |
|---|---|---|
| **A** | Bootable 32-bit kernel, GRUB 2 multiboot, VGA welcome screen, `hlt` loop | `cc89abb` |
| **Cleanup** | Fixed UTF-16 `.gitignore`, added `.gitattributes` (LF enforcement), removed committed build artifacts | `7cbe541` |
| **B.1** | GDT (5 entries), IDT (256 entries), exception handlers for vectors 0–31 | `f7930e3` |

### What the OS does right now

Boot QEMU with `cd src && make run`. You will see:

```
+===============================================================+
|        DugOS OPERATING SYSTEM  version 1.0                   |
|    [ Booted into 32-bit protected mode ]                      |
+===============================================================+

>> GDT loaded (5 entries)              ← green
>> IDT loaded (256 entries, 32 exception handlers)  ← green
>> DugOS booted. CPU halted until Phase B implements input.  ← white
```

The CPU then sits in a `hlt` loop. No input is accepted yet.

### What does NOT exist yet

- No keyboard input of any kind
- No shell or command dispatcher
- No file system
- No directory operations
- No shutdown mechanism in the kernel (the `dug_os.c` prototype has it; port to the kernel in Phase B.4)
- No hardware timer ticks (PIC is not yet remapped)
- No memory allocator

---

## Current Architecture

### Boot sequence

```
BIOS/UEFI
    │
    ▼
GRUB 2  (reads src/grub.cfg, loads dugos.elf via Multiboot 1)
    │
    ▼
_start  (src/boot.s)
    │   Sets up 16 KiB stack in BSS
    │   Calls kmain, hangs on return
    ▼
kmain   (src/main.c)
    │   vga_init()      — clears VGA buffer, sets default color
    │   welcome()       — draws the ASCII banner
    │   gdt_init()      — installs 5-entry GDT, far-jumps to reload CS
    │   idt_init()      — installs 256-entry IDT, lidt
    │   (next: intr_init, keyboard_init, shell_run ...)
    ▼
for(;;) hlt             — idle until next phase wires interrupts
```

### Memory layout (from `src/linker.ld`)

```
0x00000000 - 0x000FFFFF   BIOS / low memory (not used by DugOS)
0x00100000 (1 MiB)        Kernel load address
  .multiboot  ← MUST be first (GRUB checks the magic header)
  .text       ← kernel code
  .rodata     ← string literals (exception names, banner lines)
  .data       ← initialized globals
  .bss        ← zero-initialized (GDT, IDT arrays, stack live here)
0xB8000                   VGA text buffer (80×25, mapped directly)
```

### Interrupt handling pipeline

```
CPU exception (e.g. divide by zero)
    │
    ▼
isrN  (isr_stubs.s — ISR_NOERR or ISR_ERR macro)
    │   pushes dummy/real error code + vector number
    │   jmp isr_common
    ▼
isr_common  (isr_stubs.s)
    │   pusha, push segment regs
    │   push esp  (pointer to struct regs)
    │   call isr_common_handler
    ▼
isr_common_handler  (isr.c)
        prints "!! EXCEPTION N (name)  err=0xXX"
        cli + hlt forever
```

### Critical struct coupling

`struct regs` in `src/isr.h` must exactly match the push order in `isr_stubs.s:isr_common`:

```
Stack at handler entry (low addr first / ESP+0):
  gs, fs, es, ds         ← pushed by isr_common
  edi,esi,ebp,esp,        ← pushed by pusha
  ebx,edx,ecx,eax
  vector, err_code        ← pushed by stub before jumping
  eip, cs, eflags,       ← pushed by CPU on exception
  useresp, ss
```

**If you change one, you must change the other.** Getting this wrong produces a silent misread of register values.

---

## Important Files & Directories

### Active kernel source (`src/`)

| File | What it does | Safe to touch? |
|---|---|---|
| `main.c` | Kernel entry point. All boot stages flow through here. Add new init calls here. | ✅ Yes — this is where most Phase B.2–D work goes |
| `boot.s` | CPU entry point (`_start`). Sets up stack, calls `kmain`. Has Multiboot header. | ⚠️ Be careful — any mistake = silent triple fault |
| `vga.h / vga.c` | VGA text-mode driver. Provides `vga_write`, `vga_writeln`, `vga_write_hex`, `vga_write_dec`, `vga_set_color`. | ✅ Safe to extend |
| `gdt.h / gdt.c` | 5-entry GDT. Only needs changes if adding TSS for user-mode later. | ✅ Stable — leave alone for Phases B–D |
| `idt.h / idt.c` | IDT setup. **Phase B.2** will add IRQ entries (vectors 32–47) here. | ✅ Extend in B.2 |
| `isr.h` | `struct regs` layout — coupled to `isr_stubs.s`. | ⚠️ Must change in sync with `isr_stubs.s` |
| `isr.c` | C exception handler. Can be made recoverable later. | ✅ Safe to extend |
| `isr_stubs.s` | NASM ISR stubs + `gdt_flush` + `idt_flush`. Phase B.2 will add IRQ stubs here. | ⚠️ Assembly — review carefully, test after every change |
| `linker.ld` | Linker script. Places kernel at 1 MiB. | 🚫 Do not touch unless you fully understand multiboot memory layout |
| `grub.cfg` | GRUB menu entry. Only needs change if kernel filename changes. | ✅ Stable |
| `Makefile` | Build system. Add new `.c` and `.s` files to `C_SRCS` / `ASM_SRCS` and add a header dependency line. | ✅ Safe to extend |

### Reference material (do not modify)

| Path | What it is |
|---|---|
| `reference/minix/kernel/main.c` | The MINIX boot sequence this project mirrors |
| `reference/minix/kernel/i8259.c` | PIC remap — read this for Phase B.2 |
| `reference/minix/kernel/protect.c` | GDT/IDT pattern we followed for B.1 |
| `reference/minix/drivers/tty/keyboard.c` | PS/2 keyboard — read this for Phase B.3 |

### Behavioral prototype

| File | Purpose |
|---|---|
| `dug_os.c` | Windows userland simulation of the full OS. **Use this as the behavioral spec** for the shell commands, file ops, and dir ops you'll port in Phases B.4–D. Command names, error messages, and prompts should match this prototype. |

---

## Pending Tasks

### High priority — required for the course grade

- [ ] **B.2** — PIC remap (`intr_init`)
  - Remap 8259A PIC: IRQs 0–7 → vectors 0x20–0x27, IRQs 8–15 → vectors 0x28–0x2F
  - Reference: `reference/minix/kernel/i8259.c`
  - Add IRQ stubs to `isr_stubs.s` (same ISR_NOERR macro, vectors 32–47)
  - Wire IRQ entries into `idt.c` (extend `idt_init` or add `irq_init`)
  - Add IRQ dispatcher in `isr.c` (handler table, `isr_register_handler`)
  - Send EOI (End Of Interrupt: `outb(0x20, 0x20)`) at end of each IRQ handler
  - Uncomment `intr_init(1)` stub in `main.c` once done
  - **Visible proof:** IRQ0 timer increments a counter printed to screen

- [ ] **B.3** — PS/2 keyboard driver
  - Create `src/keyboard.h` + `src/keyboard.c` + `src/keyboard_stubs.s`
  - Register IRQ1 handler via the IRQ dispatcher from B.2
  - Read scan code from port `0x60`
  - Map scan codes → ASCII (US QWERTY layout — see `reference/minix/drivers/tty/keymaps/us-std.src`)
  - Handle shift, caps lock, backspace
  - **Visible proof:** Characters appear on screen as you type

- [ ] **B.4** — Line-buffered shell
  - Create `src/shell.h` + `src/shell.c`
  - Ring buffer or simple line buffer backed by the keyboard driver from B.3
  - Shell prompt: `  dug@os$ ` (match `dug_os.c:643`)
  - Implement built-in commands: `help`, `clear`, `shutdown`
  - `shutdown`: mask all IRQs via PIC, restore BIOS defaults (`intr_init(0)`), then halt
  - Wire `shell_run()` call into `main.c` after all init

- [ ] **C — In-memory file system**
  - Port `fwrite`, `fread`, `fedit`, `fdel` from `dug_os.c` (lines 354–455)
  - Use a flat array of file structs in BSS (no dynamic allocation needed for MVP)
  - Content stored as fixed-size char buffers (e.g., 4096 bytes per file)
  - No disk I/O required — files live only in RAM, reset on reboot
  - Implement in `src/fs.h` + `src/fs.c`
  - Also port: `rename` and `copy` commands

- [ ] **D — Directory operations**
  - Port `mkdir`, `cd`, `rmdir`, `ls` from `dug_os.c` (lines 496–557)
  - For the kernel, implement a simple tree structure (parent pointer + child list)
  - Or simpler: a flat namespace with path prefixes (easier, no tree traversal)
  - Implement in `src/dir.h` + `src/dir.c`

### Medium priority — improves quality and usability

- [ ] Add a kernel-side `outb` / `inb` port I/O helper (`src/port.h`) before B.2 so PIC and PS/2 port access is clean
- [ ] Implement hardware cursor movement in `vga.c` (write to VGA CRTC registers `0x3D4` / `0x3D5`) so the cursor tracks text output
- [ ] Fix minor VGA banner alignment — the right-edge `|` is missing on `OPERATING SYSTEM` and `version 1.0` rows (each is 65 chars instead of 67 between borders)
- [ ] Add a `vga_write_str_padded` or similar for clean table formatting in the help screen
- [ ] Add a `src/string.h` + `src/string.c` with minimal `strlen`, `strcpy`, `strcmp`, `strncpy` — you'll need these in Phase B.4 shell parsing

### Low priority — polish, deferred until E

- [ ] Color themes (`vga_set_theme()` — different palettes for banner vs. shell vs. error)
- [ ] Hardware cursor hide/show (hide during banner, show in shell input)
- [ ] Improve exception handler to show EIP and call stack (useful for debugging Phases C/D)
- [ ] Add a `date` command (read CMOS RTC via ports `0x70`/`0x71`)
- [ ] Add `whoami`, `echo`, `pwd` commands (trivial once shell exists)

### Nice-to-have (creativity rubric — Phase E)

- [ ] Interrupt-driven keyboard instead of polling (already scaffolded in B.2/B.3 — just need the handler)
- [ ] A simple color slideshow or animation on the welcome screen
- [ ] ASCII art mascot on boot
- [ ] `cat` alias for `fread`, `clear` alias for `cls`
- [ ] Command history (up arrow = previous command) via a small ring buffer

---

## Known Bugs / Technical Debt

### Active bugs

| Severity | Description | Location | Fix |
|---|---|---|---|
| **Visual** | Right-edge `\|` missing on two banner lines (`OPERATING SYSTEM`, `version 1.0`). Content is 65 chars, frame expects 67. | `src/main.c` `welcome()` | Count chars, add 2 spaces to each offending line |
| **Cosmetic** | `vga_write_hex` skips leading zeros (`0x0` prints as `0`, not `00000000`). Acceptable for now but may confuse debugging. | `src/vga.c` `vga_write_hex` | Add a `zero_padded` parameter or a separate `vga_write_hex_full` |

### Fragile code

| Area | Risk | Notes |
|---|---|---|
| `struct regs` ↔ `isr_stubs.s` | **High** — silent wrong values if push order changes | These are locked together. Document any change in both places simultaneously. |
| `boot.s` `_start` | **High** — no error recovery before stack is set up | Triple fault produces a silent reboot. Debug with `qemu -d int,cpu_reset -no-reboot`. |
| `gdt_flush` far jump | **Medium** — reloads CS; wrong GDT layout = immediate GPF | Tested and working. Do not change GDT entry order. |
| Multiboot header in `.multiboot` section | **Medium** — linker puts it first due to `linker.ld`; any change to section ordering breaks GRUB | Do not reorder sections in `linker.ld` |

### Technical debt

| Debt | Impact | When to address |
|---|---|---|
| No port I/O helpers (`outb`/`inb`) | B.2 PIC remap needs them — currently would require inline asm everywhere | Before starting B.2 |
| No string functions (`strlen`, `strcmp`, etc.) | Shell and FS will need them | Before starting B.4 |
| No memory allocator | FS file content is fixed-size; no dynamic filename/content sizing | Phase C MVP can use fixed arrays; upgrade in Phase E if time allows |
| `isr_common_handler` halts forever | Non-fatal exceptions (e.g., breakpoint) should be resumable eventually | Not needed for course; only matters if extending to user processes |
| VGA driver has no cursor management | Cursor stays at top-left; confusing in interactive shell | Add before B.4 goes live |
| `dug_os.c` uses `system("dir /B")` for `ls` | Not portable to kernel (obviously); kernel `ls` must be implemented from scratch | Address in Phase D |

### Known-working workaround (do not "fix" without testing)

- **`isr.s` renamed to `isr_stubs.s`** — this was intentional. A `.c` and `.s` file with the same base name both produce `foo.o`, causing a linker duplicate-definition error. Always use distinct base names (e.g., `keyboard.c` + `keyboard_stubs.s`).

---

## Setup Notes

### Required environment

- **OS:** WSL2 + Ubuntu 22.04 LTS (or any Linux with the packages below)
- **Not supported:** Native Windows (no `gcc -m32` or `grub-mkrescue` available without WSL)
- **Also works:** Native Ubuntu/Debian desktop, macOS with appropriate brew formulae

### Toolchain installation

```bash
# One-shot bootstrap (run once on fresh Ubuntu)
bash setup.sh
```

Manually, the packages are:

```bash
sudo apt install build-essential gcc-multilib nasm \
                 qemu-system-x86 grub-pc-bin grub-common \
                 xorriso mtools
```

### Build from the right directory

**All `make` commands must be run from `src/`**, not the repo root:

```bash
cd /mnt/c/Users/<you>/Desktop/DugOS/src
make clean && make run
```

### Adding a new source file to the build

1. Add to `C_SRCS` or `ASM_SRCS` in `src/Makefile`
2. Add a per-object header dependency line below the pattern rules:

```makefile
# Example: adding keyboard.c that includes keyboard.h and isr.h
keyboard.o: keyboard.h isr.h
```

3. If adding assembly (`_stubs.s`), ensure the base name does not collide with any `.c` file.

### No database, no external services

DugOS is entirely self-contained. There are no databases, no network services, no environment variables, and no secrets. The only external dependency is the QEMU binary.

---

## Team Conventions

### Commit message format

```
Phase X.Y: Short imperative description (subject ≤ 72 chars)

Optional body: 1–3 sentences explaining WHY, not what.
Reference the MINIX source file/line where relevant.
```

Examples:
```
Phase B.2: remap 8259A PIC, install IRQ stubs

Phase B.3: PS/2 keyboard driver, scan code to ASCII translation

Phase C: in-memory flat file system with 64-file limit
```

**Do not add** `Co-Authored-By: Claude` or any AI trailer lines.

### Branch naming

| Type | Pattern | Example |
|---|---|---|
| VM/build work | `vm/<name>` | `vm/dom` |
| OS feature | `feature/<desc>` | `feature/keyboard-driver` |
| Bug fix | `fix/<desc>` | `fix/idt-gp-fault` |
| Phase | `phase/<desc>` | `phase/b2-pic` |

### File naming rules

| Rule | Reason |
|---|---|
| NASM files paired with a `.c` file **must** have a different base name | Avoids `foo.o` collisions in Makefile pattern rules |
| Use `<module>_stubs.s` for assembly paired with `<module>.c` | Consistent with existing `isr_stubs.s` convention |
| Headers in `src/` alongside their `.c` file | No separate `include/` dir yet — keep flat until there are >10 headers |

### Code style

- **Language standard:** `gnu99` (C99 with GNU extensions)
- **No standard library** in kernel code. No `#include <stdio.h>` or `malloc`.
- **Snake_case** for all identifiers. Module-prefixed: `vga_`, `gdt_`, `idt_`, `isr_`, `kbd_`, `fs_`.
- **Comments:** Only for non-obvious WHY — hardware quirks, spec constraints, layout coupling. Never "this calls X" when the code obviously calls X.
- **Assembly:** NASM Intel syntax only. No AT&T (GNU `as`) syntax.
- **One slice per commit.** Never bundle an unrelated fix into a feature commit.

### One rule above all

**`src/main.c` is the conductor.** Every new subsystem init call goes here, in order. The commented stub list at the top of `kmain()` is the roadmap. Uncomment one at a time, verify with `make run`, commit, then move to the next.

---

## Risks & Warnings

### Files to handle with extreme care

| File | Risk level | Why |
|---|---|---|
| `src/linker.ld` | 🔴 Critical | Wrong section order silently breaks multiboot. GRUB will either not find the magic header or load the kernel at the wrong address. |
| `src/boot.s` | 🔴 Critical | This is where the CPU starts. Any error before `mov esp, stack_top` causes a triple fault with no diagnostic output. |
| `src/isr_stubs.s` (push order in `isr_common`) | 🔴 Critical | Changing push order without updating `struct regs` produces silently wrong values in the exception handler — no compile error. |
| `src/isr.h` (`struct regs` field order) | 🔴 Critical | Coupled to `isr_stubs.s`. See above. |
| `reference/minix/` | 🟡 Medium | Do not modify. These files are a read-only reference corpus. Accidentally editing one could mislead future readers into thinking it reflects DugOS behavior. |

### Potential breaking areas

| Area | Risk | Mitigation |
|---|---|---|
| Adding IRQ handlers in B.2 | If PIC is not remapped before IRQs are enabled, hardware IRQs land in vectors 8–15, overlapping CPU exception vectors. This causes immediate crashes. | Always remap PIC BEFORE enabling interrupts (`sti`). `intr_init` must be called first. |
| Enabling interrupts (`sti`) | Currently, `cli` is set in `boot.s` and never cleared. Once the keyboard driver is wired, you must call `sti` after IDT + IRQ setup. Calling it too early = crash. | Enable interrupts only after `gdt_init`, `idt_init`, and `intr_init` all succeed. |
| Stack overflow | The kernel stack is 16 KiB (`resb 16384` in `boot.s`). Deep recursion or large stack-allocated buffers can corrupt the GDT/IDT. | Avoid recursion in kernel code. Allocate large buffers in BSS, not on the stack. |
| VGA write past column 80 | `vga_putchar` wraps at column 80 and scrolls at row 25. Strings longer than 80 characters will be wrapped mid-word. | Keep all banner and shell output under 80 chars per line. |
| `make clean` in wrong directory | Running from repo root instead of `src/` does nothing (no Makefile at root). Confusing but harmless. | Always `cd src` first. Future improvement: add a root-level `Makefile` that delegates. |

### Security concerns

DugOS is an educational kernel with no security model:

- Ring 0 only — no privilege separation
- No memory protection between kernel "processes"
- No input sanitization (shell parsing is trusting)
- No network stack

This is expected and acceptable for a course project. Document it clearly if the OS is ever demonstrated outside an academic context.

### Performance concerns

None for the current phase. Performance will matter only if Phase E adds a timer-based animation or if Phase C implements a file system with many nodes. Fixed-size arrays (BSS) are fast enough for both.

---

## Recommended Next Steps

Follow this order strictly. Do not skip a phase — each one lays the foundation for the next.

### Immediate next step: Phase B.2 — PIC remap

**Why first:** Without remapping the PIC, any hardware IRQ (timer tick, keyboard press) fires at vector 8–15, which overlaps CPU exception vectors. The machine crashes instantly when any key is pressed or any timer fires.

**Concrete steps:**

1. Create `src/port.h` with inline `outb` / `inb` helpers:
   ```c
   static inline void outb(uint16_t port, uint8_t val) {
       __asm__ __volatile__ ("outb %0, %1" : : "a"(val), "Nd"(port));
   }
   static inline uint8_t inb(uint16_t port) {
       uint8_t val;
       __asm__ __volatile__ ("inb %1, %0" : "=a"(val) : "Nd"(port));
       return val;
   }
   ```

2. Create `src/pic.h` + `src/pic.c` with `pic_init()` (the real `intr_init`):
   - Send ICW1–ICW4 to master PIC (ports `0x20`, `0x21`) and slave PIC (`0xA0`, `0xA1`)
   - Remap: master IRQs 0–7 → vectors 0x20–0x27, slave IRQs 8–15 → vectors 0x28–0x2F
   - Reference: `reference/minix/kernel/i8259.c`

3. Add IRQ stubs to `isr_stubs.s` (vectors 32–47 using `ISR_NOERR`)

4. Add IRQ entries to `idt.c`

5. Add an IRQ handler dispatcher to `isr.c`:
   ```c
   void (*irq_handlers[16])(void) = {0};
   void irq_register(uint8_t irq, void (*handler)(void));
   ```

6. Uncomment `intr_init(1)` call in `main.c`

7. Verify: `make run` still shows the same boot output, no crashes

### Then: Phase B.3 — Keyboard

Reference `reference/minix/drivers/tty/keyboard.c` for scan code tables.

### Then: Phase B.4 — Shell

Port command dispatch from `dug_os.c:589` (`shell_dispatch`) into `src/shell.c`. Match the command names and error messages in the prototype exactly — the grader may test specific commands.

### Then: Phase C and D

The behavioral spec is `dug_os.c`. Every command that exists in the prototype should exist in the kernel shell. The key difference: replace `fopen`/`fread`/`mkdir`/`chdir` calls with your own in-memory implementations.

---

## Questions for Future Developers

These are open architecture questions that need a team decision before implementation:

1. **In-memory FS structure for Phase C:** Flat array of file structs (simple, ~30 lines) or a proper tree with dynamic nodes (more realistic, needs allocator)? Recommendation: flat array for MVP, tree as Phase E enhancement.

2. **Memory allocator:** Phase C with a flat array needs no allocator. If you go with a tree FS, you need a simple bump allocator (~20 lines). Decision needed before starting C.

3. **Shutdown implementation:** The prototype calls `intr_init(0)` (restores BIOS PIC defaults) then exits the process. In the kernel, real shutdown on QEMU can be triggered via port `0x604` (QEMU-specific power off) or `0xB004` (older QEMU). Is QEMU-specific shutdown OK, or do we need ACPI? Recommendation: QEMU port `0x604` for now.

4. **User mode (ring 3):** The GDT has user-mode segments stubbed (entries 3 and 4). Are we planning to run any process in ring 3, or is everything ring 0 for this course? This affects whether we need a TSS, syscall entry, or user-space stack. Recommendation: ring 0 only for CMSC 125.

5. **Directory structure for Phase D:** Real directories (tree with parent/child pointers) or flat path-prefix namespace (e.g., every file stores its full path as a string)? Recommendation: flat prefix namespace for MVP — it's ~40 lines and satisfies the rubric.

6. **PS/2 or polling for keyboard?** The current plan is interrupt-driven (IRQ1). Polling the PS/2 data port (`0x60`) in a loop also works and skips B.2 entirely. This is a schedule risk decision for the team.

---

## Repository Cleanup Suggestions

1. **Add a root-level `Makefile`** that delegates to `src/Makefile`:
   ```makefile
   .PHONY: all run clean
   all run clean:
       $(MAKE) -C src $@
   ```
   Lets the team run `make run` from anywhere in the repo.

2. **Move `dug_os.c` and `dug_os.exe`** into `reference/prototype/` so the repo root is clearly "kernel source + docs only."

3. **Tag each completed phase** in git:
   ```bash
   git tag phase-A cc89abb
   git tag phase-B1 f7930e3
   ```
   Graders can checkout exact milestones. Also useful for bisecting regressions.

4. **Add `.editorconfig`** to enforce tab indentation for Makefiles and 4-space indent for C/NASM across all editors automatically.

---

## Suggested Refactors

| Refactor | When | Effort |
|---|---|---|
| Extract `outb`/`inb` into `src/port.h` | Before B.2 | 10 min |
| Extract `strlen`, `strcmp`, `strncpy` into `src/string.h` + `src/string.c` | Before B.4 | 30 min |
| Make `isr_common_handler` dispatch to a registered handler table instead of always halting | After B.2 | 1 hour |
| Replace banner `vga_writeln` calls with a loop over a string array | Phase E polish | 30 min |
| Add `vga_write_hex_full` (zero-padded 8-digit hex) | When needed for debugging | 15 min |

---

## Suggested Documentation Files to Create Next

| File | Purpose | Priority |
|---|---|---|
| `ARCHITECTURE.md` | Memory map diagram, GDT/IDT layout table, boot sequence with addresses, planned FS layout | High — needed before Phase C |
| `CONTRIBUTING.md` | PR checklist, how to add a new command, coding standards for new teammates | Medium |
| `CHANGELOG.md` | Per-phase changelog linked to commit hashes — useful for oral defense | Medium |
| `docs/minix-mapping.md` | Table: MINIX source file → DugOS equivalent. Helps justify design decisions to instructor. | Medium |
| `SECURITY.md` | Scope disclaimer: no privilege separation, no network, educational only | Low |

---

*This document covers the state of the repository as of commit `f7930e3` (Phase B.1). Update the status table and pending task checklist as each phase is completed.*
