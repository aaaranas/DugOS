# DugOS — Improvements & Polish Guide

**Based on:** TASK_HANDOFF.md requirements, project spec Req 5–6, creativity rubric (Phase E)
**Current state:** Phases A, B.1, B.2, B.3, B.4, C, D — all complete and pushed to `main`.

---

## Priority Order (if time is limited)

| # | Item | Effort | Why it matters |
|---|---|---|---|
| 1 | Fix banner alignment | 5 min | Grader sees it immediately on boot |
| 2 | Update TASK_HANDOFF.md checkboxes | 5 min | Document hygiene — grader may read it |
| 3 | Add `echo` command | 5 min | Free creativity points |
| 4 | Hardware VGA cursor | 20 min | Makes shell look like a real terminal |
| 5 | IRQ0 timer tick counter | 30 min | Spec explicitly asks for "visible proof" |
| 6 | `date` command (CMOS RTC) | 1 hr | Demonstrates hardware port I/O knowledge |
| 7 | Command history (Up arrow) | 1 hr | Polish — shows extra effort |
| 8 | Color themes | 30 min | Visual impression on demo |

---

## 1. Known Visual Bugs *(grader will see these on boot)*

### Banner alignment — missing right-edge `|`

The TASK_HANDOFF explicitly flags this bug. Two lines in `welcome()` in `src/main.c` are 65 characters wide between the border pipes instead of the required 67, so the right `|` appears misaligned.

**File:** `src/main.c` — `welcome()` function

```c
// CURRENT (wrong — 65 chars between borders):
vga_writeln("  |          O P E R A T I N G     S Y S T E M                    |");
vga_writeln("  |                      v e r s i o n  1.0                        |");

// FIXED (67 chars between borders — matches the frame):
vga_writeln("  |          O P E R A T I N G     S Y S T E M                      |");
vga_writeln("  |                      v e r s i o n  1.0                          |");
```

### `vga_write_hex` skips leading zeros

`0x0` prints as `0` instead of `00000000`. Low impact but can confuse debugging output in the exception handler.

**File:** `src/vga.c` — `vga_write_hex()`

Fix: always print all 8 nibbles (remove the `started` flag, always output).

---

## 2. Update TASK_HANDOFF.md Checkboxes

All `[ ]` items under **Pending Tasks** are still unchecked even though Phases B.2–D are fully implemented. A grader reading the document will think work is incomplete.

Change every completed item from `[ ]` to `[x]` in `TASK_HANDOFF.md`:

```markdown
- [x] **B.2** — PIC remap
- [x] **B.3** — PS/2 keyboard driver
- [x] **B.4** — Line-buffered shell
- [x] **C** — FAT-based in-memory file system
- [x] **D** — Directory operations
```

Also update the status table at the top of the file and the footer timestamp.

---

## 3. Missing `echo` Command *(3 lines — free creativity points)*

The `dug_os.c` prototype lists `echo` as an extra feature. It is trivial to add.

**File:** `src/shell.c` — inside `shell_dispatch()`

```c
} else if (kstrcmp(cmd, "echo") == 0) {
    vga_write("  ");
    for (int i = 1; i < argc; i++) {
        vga_write(argv[i]);
        if (i < argc - 1) vga_putchar(' ');
    }
    vga_writeln("");
```

Also add it to the help screen in `cmd_help()`:

```c
vga_writeln("    echo   <text>          Print text to the screen");
```

---

## 4. Hardware VGA Cursor *(makes shell look like a real terminal)*

Right now the blinking cursor stays at the top-left corner and never moves. Every character typed appears correctly but the cursor position is wrong, which looks broken during interactive input.

Fixing it requires writing the cursor position to VGA CRTC registers `0x3D4`/`0x3D5` after each character is written.

**File:** `src/vga.c`

Add at the top (after the existing includes):
```c
#include "port.h"   /* outb for CRTC cursor registers */
```

Add a new internal function:
```c
static void update_cursor(void)
{
    uint16_t pos = (uint16_t)(row * VGA_WIDTH + col);
    outb(0x3D4, 0x0F); outb(0x3D5, (uint8_t)(pos & 0xFF));   /* low byte  */
    outb(0x3D4, 0x0E); outb(0x3D5, (uint8_t)(pos >> 8));     /* high byte */
}
```

Call `update_cursor()` at the end of `vga_putchar()` (after all the `if/else` branches).

Also update the Makefile dependency:
```makefile
vga.o: vga.h port.h
```

---

## 5. IRQ0 Timer Tick — "Visible Proof" the Spec Asks For

The TASK_HANDOFF states under Phase B.2:
> **Visible proof:** IRQ0 timer increments a counter printed to screen

IRQ0 is already wired (the stub exists and EOI is sent) but no handler is registered so nothing happens. Adding a tick counter displayed in the top-right corner of the VGA screen directly satisfies this requirement.

**New file:** `src/timer.h` + `src/timer.c`

```c
/* timer.h */
void timer_init(void);      /* register IRQ0 handler */
uint32_t timer_ticks(void); /* return current tick count */
```

```c
/* timer.c */
#include "timer.h"
#include "isr.h"   /* irq_register */
#include "vga.h"   /* direct VGA write for top-right counter */

#define VGA_MEM ((volatile uint16_t *)0xB8000)
#define VGA_WIDTH 80

static volatile uint32_t ticks = 0;

static void timer_isr(void)
{
    ticks++;
    /* Write tick count to top-right corner of VGA (col 72-79, row 0). */
    /* Use vga_write_dec equivalent but write directly to avoid cursor movement. */
}

void timer_init(void)  { irq_register(0, timer_isr); pic_unmask(0); }
uint32_t timer_ticks(void) { return ticks; }
```

Wire `timer_init()` in `src/main.c` after `kbd_init()`.

---

## 6. `date` Command — CMOS Real-Time Clock

The TASK_HANDOFF lists this as a low-priority polish item. It demonstrates hardware port I/O knowledge and makes the OS feel more complete.

The CMOS RTC is read via two ports:
- `0x70` — index register (select which CMOS register to read)
- `0x71` — data register (read the selected register)

**File:** `src/shell.c` — add to `shell_dispatch()`:

```c
} else if (kstrcmp(cmd, "date") == 0) {
    cmd_date();
```

**New function** in `shell.c`:

```c
static uint8_t cmos_read(uint8_t reg)
{
    outb(0x70, reg);
    return inb(0x71);
}

/* Convert BCD-encoded CMOS byte to binary. */
static uint8_t bcd(uint8_t v) { return (v >> 4) * 10 + (v & 0x0F); }

static void cmd_date(void)
{
    uint8_t sec  = bcd(cmos_read(0x00));
    uint8_t min  = bcd(cmos_read(0x02));
    uint8_t hour = bcd(cmos_read(0x04));
    uint8_t day  = bcd(cmos_read(0x07));
    uint8_t mon  = bcd(cmos_read(0x08));
    uint8_t year = bcd(cmos_read(0x09));

    vga_write("  20"); vga_write_dec(year); vga_write("-");
    if (mon  < 10) vga_write("0"); vga_write_dec(mon);  vga_write("-");
    if (day  < 10) vga_write("0"); vga_write_dec(day);  vga_write("  ");
    if (hour < 10) vga_write("0"); vga_write_dec(hour); vga_write(":");
    if (min  < 10) vga_write("0"); vga_write_dec(min);  vga_write(":");
    if (sec  < 10) vga_write("0"); vga_write_dec(sec);
    vga_writeln("");
}
```

Needs `#include "port.h"` in `shell.c`.

---

## 7. Command History (Up Arrow)

The TASK_HANDOFF lists this as a "nice-to-have" under the creativity rubric. It requires:

1. A ring buffer of the last N command strings (e.g. 8 entries × 256 bytes = 2KB).
2. Detecting the Up arrow escape sequence from the PS/2 keyboard.
3. In `readline()`, when Up is pressed: clear the current line on screen and fill the buffer with the previous command.

**PS/2 Up arrow scan code:** `0x48` (make code). Currently `keyboard.c` maps this to `0` (ignored). Change it to emit a special sentinel byte (e.g. `0x01` = unused ASCII SOH) that `readline()` recognizes as "Up arrow pressed."

**Effort:** ~1 hour. **Payoff:** high impression on demo and oral defense.

---

## 8. Color Themes

The TASK_HANDOFF lists `vga_set_theme()` as a Phase E extra. Minimal implementation:

Define three named themes and apply them at the right moments:

```c
/* In vga.h */
void vga_theme_banner(void);  /* yellow on black  — boot banner     */
void vga_theme_shell(void);   /* white on black   — normal shell    */
void vga_theme_error(void);   /* red on black     — error messages  */
void vga_theme_success(void); /* green on black   — success output  */
```

These are just wrappers around `vga_set_color()` that give the theme a name so future developers can change the palette in one place. Already partially implemented (colors are set inline in each subsystem).

---

## Summary Checklist

```
[ ] Fix banner right-edge | alignment in src/main.c
[ ] Update TASK_HANDOFF.md checkboxes to [x] for B.2-D
[ ] Add echo command to src/shell.c
[ ] Add hardware VGA cursor to src/vga.c
[ ] Add IRQ0 timer tick counter (src/timer.h + src/timer.c)
[ ] Add date command via CMOS RTC to src/shell.c
[ ] Add command history (Up arrow) to src/shell.c + src/keyboard.c
[ ] Add named color theme functions to src/vga.h + src/vga.c
```

---

*Generated against commit `f959b09` (Phase B.2-D complete). Update this file as items are checked off.*
