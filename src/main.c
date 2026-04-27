/* DugOS -- kernel entry point.
 *
 * Mirrors MINIX 3 kernel/main.c (Appendix B, line 07100).
 * Phase A: only vga_init + welcome + idle loop are live.
 * Later phases will uncomment intr_init, the boot tables, etc.
 */

#include "vga.h"
#include "gdt.h"
#include "idt.h"

static void welcome(void);

/* ---- Stubs to be uncommented in later phases ----------------
 * static void intr_init(int minit);
 * static void boot_proc_table(void);
 * static void boot_priv_table(void);
 * static void boot_image_init(void);
 * static void announce(void);
 * -------------------------------------------------------------- */

void kmain(void)
{
    vga_init();
    welcome();

    /* Phase B.1 -- protected-mode tables. */
    gdt_init();
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_writeln("  >> GDT loaded (5 entries)");

    idt_init();
    vga_writeln("  >> IDT loaded (256 entries, 32 exception handlers)");

    /* To prove the IDT works, uncomment the next line -- it raises
     * a breakpoint exception (vector 3) and triggers isr_common_handler. */
    /* __asm__ __volatile__ ("int $3"); */

    /* Phase B.2+ -- uncomment as each is implemented:
     *
     *   intr_init(1);
     *   boot_proc_table();
     *   boot_priv_table();
     *   boot_image_init();
     *   announce();
     */

    vga_writeln("");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_writeln("  >> DugOS booted. CPU halted until Phase B implements input.");

    /* Hard requirement #2: infinite loop until shutdown. */
    for (;;) {
        __asm__ __volatile__ ("hlt");
    }
}

static void welcome(void)
{
    vga_set_color(VGA_YELLOW, VGA_BLACK);
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
