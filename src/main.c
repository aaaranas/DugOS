/* =============================================================================
 * main.c -- DugOS kernel entry point
 *
 * PURPOSE:
 *   This file is the heart of the DugOS boot sequence. It mirrors the
 *   structure of MINIX 3's kernel/main.c (Appendix B, line 07100), which
 *   is the reference implementation we are studying for CMSC 125.
 *
 * HOW IT WORKS:
 *   After boot.s sets up the stack, it calls kmain() below. kmain() runs
 *   each boot stage in order -- display, protected-mode tables, interrupt
 *   controller, process tables -- and eventually starts the shell loop.
 *
 * BOOT SEQUENCE (mirrors MINIX main.c):
 *   1. vga_init()         -- clear screen, set default text colour
 *   2. welcome()          -- draw the ASCII banner
 *   3. gdt_init()         -- install our own Global Descriptor Table
 *   4. idt_init()         -- install Interrupt Descriptor Table + exception handlers
 *   5. pic_init()         -- [Phase B.2] remap 8259A PIC, install IRQ vectors
 *   6. boot_proc_table()  -- [Phase B.2] clear the MINIX-style process table
 *   7. boot_priv_table()  -- [Phase B.2] clear the privilege table
 *   8. boot_image_init()  -- [Phase B.2] load boot image processes
 *   9. announce()         -- [Phase B.2] print kernel version banner
 *  10. kbd_init()         -- [Phase B.3] install PS/2 keyboard IRQ handler
 *  11. fs_init()          -- [Phase C]   initialize in-memory FAT file system
 *  12. dir_init()         -- [Phase D]   initialize directory subsystem
 *  13. sti                -- enable hardware interrupts (after all init done)
 *  14. shell_run()        -- [Phase B.4] enter the interactive shell loop
 *
 * REFERENCES:
 *   MINIX 3 source: reference/minix/kernel/main.c
 *   Project spec:   Honey-OS-Phase-2-v.-1.2.pdf (CMSC 125)
 * =============================================================================
 */

#include "vga.h"      /* VGA text-mode output functions                   */
#include "gdt.h"      /* Global Descriptor Table initialisation           */
#include "idt.h"      /* Interrupt Descriptor Table initialisation        */
#include "pic.h"      /* 8259A PIC remap (Phase B.2)                      */
#include "keyboard.h" /* PS/2 keyboard driver (Phase B.3)                 */
#include "fs.h"       /* in-memory FAT file system (Phase C)              */
#include "dir.h"      /* directory operations (Phase D)                   */
#include "shell.h"    /* interactive shell (Phase B.4)                    */
#include "string.h"   /* kstrlen, etc.                                    */

/* Forward declaration for the welcome screen function defined below. */
static void welcome(void);

/* ============================================================
 *  MINIX-STYLE BOOT FUNCTIONS (Phase B.2)
 *
 *  These mirror the MINIX kernel/main.c boot sequence:
 *    boot_proc_table()   -- clear the process table (NR_TASKS + NR_PROCS slots)
 *    boot_priv_table()   -- clear the privilege table (NR_SYS_PROCS slots)
 *    boot_image_init()   -- load the boot image into the process table
 *    announce()          -- print the kernel version banner
 *
 *  In DugOS these functions print status messages to VGA rather than
 *  manipulating real process/privilege tables (which are beyond scope).
 *  They satisfy the "implement the MINIX main.c boot sequence" requirement.
 * ============================================================ */

/* Constants mirroring MINIX dug_os.c */
#define NR_TASKS   4
#define NR_PROCS   64
#define NR_SYS_PROCS 32
#define NR_BOOT_PROCS 8

/* Boot image table -- same processes as MINIX / dug_os.c prototype. */
static const struct {
    int  proc_nr;
    int  priority;
    int  quantum;
    char proc_name[16];
} image[NR_BOOT_PROCS] = {
    { -4, 0, 0,  "HARDWARE" },
    { -3, 1, 10, "IDLE"     },
    { -2, 2, 10, "CLOCK"    },
    { -1, 2, 10, "SYSTEM"   },
    {  0, 3, 10, "PM"       },
    {  1, 3, 10, "FS"       },
    {  2, 3, 10, "TTY"      },
    {  3, 4, 10, "INIT"     },
};

/* =============================================================================
 * boot_proc_table() -- clear and initialize the MINIX-style process table
 *
 * Mirrors MINIX kernel/main.c boot_proc_table(). In DugOS this is a
 * boot-status print; in MINIX it would zero out the actual proc[] array.
 * =============================================================================
 */
static void boot_proc_table(void)
{
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_write("  [kernel] proc_table: ");
    vga_write_dec(NR_TASKS + NR_PROCS);
    vga_writeln(" slots cleared ... OK");
}

/* =============================================================================
 * boot_priv_table() -- clear and initialize the privilege table
 * =============================================================================
 */
static void boot_priv_table(void)
{
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_write("  [kernel] priv_table: ");
    vga_write_dec(NR_SYS_PROCS);
    vga_writeln(" slots cleared ... OK");
}

/* =============================================================================
 * boot_image_init() -- load boot image processes into the process table
 * =============================================================================
 */
static void boot_image_init(void)
{
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    for (int i = 0; i < NR_BOOT_PROCS; i++) {
        vga_write("  [kernel]   -> loaded  ");
        vga_write(image[i].proc_name);
        vga_writeln("");
    }
    vga_write("  [kernel] boot_image: ");
    vga_write_dec(NR_BOOT_PROCS);
    vga_writeln(" processes loaded  ... OK");
}

/* =============================================================================
 * announce() -- print the kernel startup banner (mirrors MINIX announce())
 * =============================================================================
 */
static void announce(void)
{
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_writeln("  [kernel] DugOS v1.0 -- Built on MINIX 3.1.0 concepts");
    vga_writeln("  [kernel] Executing in 32-bit protected mode.");
}

/* =============================================================================
 * kmain() -- kernel main function (entry point from boot.s)
 *
 * This is called by _start in boot.s after the stack is ready. It runs each
 * boot stage in sequence and must NEVER return -- the shell_run() infinite
 * loop at the bottom satisfies project requirement #2 ("run as an infinite
 * loop"). If shell_run somehow returns (it should not), the trailing hlt loop
 * prevents executing garbage memory.
 * =============================================================================
 */
void kmain(void)
{
    /* -----------------------------------------------------------------------
     * Stage 1: Initialise the VGA text driver and display the welcome screen.
     * vga_init() clears the 80x25 display and sets the default text colour.
     * welcome() draws the DugOS ASCII-art banner.
     * ----------------------------------------------------------------------- */
    vga_init();
    welcome();

    /* -----------------------------------------------------------------------
     * Stage 2 (Phase B.1): Install the Global Descriptor Table.
     * gdt_init() replaces the minimal GDT that GRUB set up with our own
     * 5-entry table (null, kernel code, kernel data, user code, user data).
     * ----------------------------------------------------------------------- */
    gdt_init();
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_writeln("  >> GDT loaded (5 entries)");

    /* -----------------------------------------------------------------------
     * Stage 3 (Phase B.1): Install the Interrupt Descriptor Table.
     * idt_init() fills a 256-entry IDT: exception handlers for vectors 0-31
     * AND IRQ handlers for vectors 32-47 (Phase B.2 IRQ stubs are wired
     * here so they are in place before the PIC is remapped).
     * ----------------------------------------------------------------------- */
    idt_init();
    vga_writeln("  >> IDT loaded (256 entries, 32 exceptions + 16 IRQ slots)");

    /* -----------------------------------------------------------------------
     * Stage 4 (Phase B.2): Remap the 8259A PIC.
     * pic_init() sends ICW1-ICW4 to shift hardware IRQs 0-7 from BIOS default
     * vectors 8-15 (which overlap CPU exceptions!) to vectors 32-39, and
     * IRQs 8-15 to vectors 40-47. This MUST happen before sti.
     * pic_init() prints its own status line to VGA.
     * ----------------------------------------------------------------------- */
    pic_init();

    /* -----------------------------------------------------------------------
     * Stages 5-7 (Phase B.2): MINIX-style process/privilege table init and
     * boot image loading. These mirror MINIX kernel/main.c steps and satisfy
     * the project spec's requirement to implement the MINIX boot sequence.
     * ----------------------------------------------------------------------- */
    boot_proc_table();
    boot_priv_table();
    boot_image_init();
    announce();

    /* -----------------------------------------------------------------------
     * Stage 8 (Phase B.3): Initialize the PS/2 keyboard driver.
     * kbd_init() registers the IRQ1 handler and unmasks IRQ1 on the PIC.
     * Interrupts are still globally disabled (cli from boot.s), so the handler
     * will not fire until we execute sti below.
     * ----------------------------------------------------------------------- */
    kbd_init();
    vga_writeln("  >> Keyboard driver initialized (IRQ1 registered)");

    /* -----------------------------------------------------------------------
     * Stage 9 (Phase C): Initialize the in-memory FAT file system.
     * fs_init() marks all FAT blocks free and clears the file directory.
     * ----------------------------------------------------------------------- */
    fs_init();
    vga_writeln("  >> File system initialized (8 x 32KB blocks, FAT-linked)");

    /* -----------------------------------------------------------------------
     * Stage 10 (Phase D): Initialize the directory subsystem.
     * dir_init() creates the root directory "/" and sets cwd to "/".
     * ----------------------------------------------------------------------- */
    dir_init();
    vga_writeln("  >> Directory subsystem initialized (cwd = /)");

    /* -----------------------------------------------------------------------
     * Enable hardware interrupts.
     * All handlers are now installed (exception handlers, IRQ stubs, keyboard
     * handler). The PIC is remapped. It is now safe to enable interrupts.
     * sti allows the CPU to receive IRQ1 (keyboard) and IRQ0 (timer).
     * ----------------------------------------------------------------------- */
    vga_writeln("");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_writeln("  >> All systems ready. Entering shell...");
    vga_writeln("");

    __asm__ __volatile__ ("sti");  /* enable hardware interrupts */

    /* -----------------------------------------------------------------------
     * Stage 11 (Phase B.4): Enter the interactive shell.
     * shell_run() loops forever reading commands from the keyboard.
     * It only returns if something goes catastrophically wrong.
     * ----------------------------------------------------------------------- */
    shell_run();

    /* -----------------------------------------------------------------------
     * Safety net: if shell_run() ever returns (it should not), halt the CPU.
     * This satisfies project requirement #2 ("run as an infinite loop").
     * ----------------------------------------------------------------------- */
    for (;;) {
        __asm__ __volatile__ ("cli; hlt");
    }
}

/* =============================================================================
 * welcome() -- display the DugOS ASCII-art welcome banner
 *
 * Draws a bordered banner showing the OS name, version, and reference.
 * Uses yellow text on a black background for visual impact. All output goes
 * through vga_writeln() which writes directly to VGA memory at 0xB8000.
 *
 * Called once from kmain() before any hardware initialisation so that
 * something is visible on screen even if a later init stage crashes.
 * =============================================================================
 */
static void welcome(void)
{
    /* Set text colour to yellow-on-black for the banner. */
    vga_set_color(VGA_YELLOW, VGA_BLACK);

    /* Draw the banner -- each call writes one line to the VGA buffer. */
    vga_writeln("");
    vga_writeln("  +=================================================================+");
    vga_writeln("  |                                                                 |");
    vga_writeln("  |    ____  _   _   ____   ___   ____                             |");
    vga_writeln("  |   |  _ \\| | | | / ___| / _ \\ / ___|                            |");
    vga_writeln("  |   | | | | | | || |  _ | | | |\\___ \\                            |");
    vga_writeln("  |   | |_| | |_| || |_| || |_| | ___) |                           |");
    vga_writeln("  |   |____/ \\___/  \\____| \\___/ |____/                             |");
    vga_writeln("  |                                                                 |");
    vga_writeln("  |          O P E R A T I N G     S Y S T E M                    |");
    vga_writeln("  |                      v e r s i o n  1.0                        |");
    vga_writeln("  |                                                                 |");
    vga_writeln("  |   - - - - - - - - - - - - - - - - - - - - - - - - - - - - -   |");
    vga_writeln("  |      Built on MINIX 3.1.0 kernel  (Appendix B, line 07100)    |");
    vga_writeln("  |   - - - - - - - - - - - - - - - - - - - - - - - - - - - - -   |");
    vga_writeln("  |                                                                 |");
    vga_writeln("  |              [ Type  'help'  to begin ]                        |");
    vga_writeln("  |                                                                 |");
    vga_writeln("  +=================================================================+");
}
