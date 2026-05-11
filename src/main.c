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
 *   Most stages are currently commented out; they are uncommented one at a
 *   time as each phase is implemented and verified.
 *
 * BOOT SEQUENCE (mirrors MINIX main.c):
 *   1. vga_init()         -- clear screen, set default text colour
 *   2. welcome()          -- draw the ASCII banner
 *   3. gdt_init()         -- install our own Global Descriptor Table
 *   4. idt_init()         -- install Interrupt Descriptor Table + exception handlers
 *   5. intr_init(1)       -- [Phase B.2] remap 8259A PIC, enable hardware IRQs
 *   6. boot_proc_table()  -- [Phase B.2] clear the MINIX-style process table
 *   7. boot_priv_table()  -- [Phase B.2] clear the privilege table
 *   8. boot_image_init()  -- [Phase B.2] load boot image processes
 *   9. announce()         -- [Phase B.2] print kernel version banner
 *  10. shell_run()        -- [Phase B.4] enter the interactive shell loop
 *
 * REFERENCES:
 *   MINIX 3 source: reference/minix/kernel/main.c
 *   Project spec:   Honey-OS-Phase-2-v.-1.2.pdf (CMSC 125)
 * =============================================================================
 */

#include "vga.h"   /* VGA text-mode output functions                  */
#include "gdt.h"   /* Global Descriptor Table initialisation          */
#include "idt.h"   /* Interrupt Descriptor Table initialisation       */

/* Forward declaration for the welcome screen function defined below. */
static void welcome(void);

/* =============================================================================
 * STUB FUNCTIONS -- to be uncommented as each phase is implemented.
 *
 * Following the project spec requirement: "implement the first couple of
 * functions, then comment out the other function calls so that you can focus
 * on understanding how to boot an OS. Later uncomment them as you progress."
 *
 * Each stub listed here corresponds to a MINIX kernel function that will be
 * ported and wired in during the indicated phase.
 * =============================================================================
 * static void intr_init(int minit);   -- Phase B.2: remap 8259A PIC
 * static void boot_proc_table(void);  -- Phase B.2: clear process table
 * static void boot_priv_table(void);  -- Phase B.2: clear privilege table
 * static void boot_image_init(void);  -- Phase B.2: load boot image entries
 * static void announce(void);         -- Phase B.2: print startup message
 * static void shell_run(void);        -- Phase B.4: interactive command shell
 */

/* =============================================================================
 * kmain() -- kernel main function (entry point from boot.s)
 *
 * This is called by _start in boot.s after the stack is ready. It runs each
 * boot stage in sequence and must NEVER return -- the for(;;) hlt loop at
 * the bottom satisfies project requirement #2 ("run as an infinite loop").
 * =============================================================================
 */
void kmain(void)
{
    /* -- Stage 1: Initialise the VGA text driver and display the welcome screen.
     *    vga_init() clears the 80x25 display and sets the default text colour.
     *    welcome() draws the DugOS ASCII-art banner. */
    vga_init();
    welcome();

    /* -- Stage 2 (Phase B.1): Install the Global Descriptor Table.
     *    gdt_init() replaces the minimal GDT that GRUB set up with our own
     *    5-entry table (null, kernel code, kernel data, user code, user data).
     *    After this call, all memory segment registers point to flat 4 GiB
     *    segments and the CPU segment protection is under our control. */
    gdt_init();
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);  /* switch to green for status lines */
    vga_writeln("  >> GDT loaded (5 entries)");

    /* -- Stage 3 (Phase B.1): Install the Interrupt Descriptor Table.
     *    idt_init() fills a 256-entry IDT and installs exception handlers for
     *    CPU vectors 0-31 (divide-by-zero, page fault, GPF, etc.). Without
     *    this, any CPU fault causes a triple fault and silent reboot. */
    idt_init();
    vga_writeln("  >> IDT loaded (256 entries, 32 exception handlers)");

    /* -- IDT self-test (optional): uncomment to raise a breakpoint exception
     *    (vector 3) and verify that isr_common_handler prints a red message.
     *    Re-comment this line after testing. */
    /* __asm__ __volatile__ ("int $3"); */

    /* -- Stages 4-9 (Phase B.2 and beyond): uncomment one at a time as each
     *    is implemented. Each function must be tested with make run before
     *    uncommenting the next, following the project spec methodology. */
    /*
     *   intr_init(1);        -- remap 8259A PIC, enable hardware IRQs
     *   boot_proc_table();   -- initialise the MINIX-style process table
     *   boot_priv_table();   -- initialise the privilege table
     *   boot_image_init();   -- populate table with boot image processes
     *   announce();          -- print OS version banner to screen
     *   shell_run();         -- enter interactive shell (Phase B.4)
     */

    /* Final boot status line -- printed after all init stages complete. */
    vga_writeln("");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_writeln("  >> DugOS booted. CPU halted until Phase B implements input.");

    /* -- Project requirement #2: "Your OS will be run as an infinite loop."
     *    The hlt instruction pauses the CPU until the next interrupt, reducing
     *    power consumption while still satisfying the infinite-loop requirement.
     *    Once the keyboard driver is ready (Phase B.3), the shell_run() call
     *    above will replace this loop. */
    for (;;) {
        __asm__ __volatile__ ("hlt");
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
    vga_writeln("  |    ____  _   _   ____   ___   ____                              |");
    vga_writeln("  |   |  _ \\| | | | / ___| / _ \\ / ___|                             |");
    vga_writeln("  |   | | | | | | || |  _ | | | |\\___ \\                             |");
    vga_writeln("  |   | |_| | |_| || |_| || |_| | ___) |                            |");
    vga_writeln("  |   |____/ \\___/  \\____| \\___/ |____/                             |");
    vga_writeln("  |                                                                 |");
    vga_writeln("  |          O P E R A T I N G     S Y S T E M                     |");
    vga_writeln("  |                      v e r s i o n  1.0                         |");
    vga_writeln("  |                                                                 |");
    vga_writeln("  |   - - - - - - - - - - - - - - - - - - - - - - - - - - - - -    |");
    vga_writeln("  |       Built on MINIX 3.1.0 kernel  (Appendix B, line 07100)     |");
    vga_writeln("  |   - - - - - - - - - - - - - - - - - - - - - - - - - - - - -    |");
    vga_writeln("  |                                                                 |");
    vga_writeln("  |              [ Booted into 32-bit protected mode ]              |");
    vga_writeln("  |                                                                 |");
    vga_writeln("  +=================================================================+");
}
