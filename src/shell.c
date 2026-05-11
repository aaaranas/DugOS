/* =============================================================================
 * shell.c -- DugOS interactive command shell (Phase B.4)
 *
 * PURPOSE:
 *   Implements the interactive shell that lets the user interact with DugOS.
 *   It reads one line of input at a time from the PS/2 keyboard, parses it
 *   into a command and arguments, and dispatches to the appropriate subsystem
 *   (VGA clear, file system, directory operations, shutdown, etc.).
 *
 * DESIGN:
 *   shell_run() is the main loop: show prompt, read line, dispatch, repeat.
 *   readline() reads one line character by character via kbd_getchar().
 *   parse_cmd() splits the line into up to MAX_ARGS tokens in-place.
 *   shell_dispatch() matches the command token and calls the handler.
 *
 * COMMAND REFERENCE (matches dug_os.c prototype exactly):
 *   General:       help  clear/cls  pwd  whoami  shutdown
 *   File ops:      fwrite <f>  fread/cat <f>  fedit <f>  fdel <f>
 *                  rename <old> <new>  copy <src> <dst>
 *   Directory:     mkdir <d>  cd <d>  rmdir <d>  ls/dir
 *
 * SHUTDOWN:
 *   Restores BIOS PIC defaults via pic_restore(), then sends the QEMU
 *   power-off command via port 0x604. On older QEMU also tries 0xB004.
 *
 * REFERENCES:
 *   dug_os.c lines 589-654 -- shell_dispatch and shell_run behavioral spec
 *   dug_os.c lines 289-323 -- cmd_help() text we mirror
 *   TASK_HANDOFF.md Phase B.4 -- shutdown notes (QEMU port 0x604)
 * =============================================================================
 */

#include "shell.h"
#include "vga.h"       /* all VGA output functions                             */
#include "keyboard.h"  /* kbd_getchar()                                        */
#include "string.h"    /* kstrlen, kstrcmp, kstrncpy, kmemset                 */
#include "fs.h"        /* fs_write, fs_read, fs_edit, fs_delete, fs_rename,   */
                       /* fs_copy (file operations -- Phase C)                 */
#include "dir.h"       /* dir_mkdir, dir_cd, dir_rmdir, dir_ls, dir_getcwd    */
#include "pic.h"       /* pic_restore() for shutdown                           */
#include "port.h"      /* outb() for QEMU power-off port                       */

/* ---- Shell configuration ---------------------------------------------------*/
#define CMD_LEN   256   /* maximum command line length (chars)  */
#define MAX_ARGS  8     /* maximum tokens per command line       */

/* ---- Boot image table (mirrors dug_os.c -- used by shutdown display) -------*/
#define NR_BOOT_PROCS 8
#define P_NAME_LEN    16

static const struct {
    int  proc_nr;
    char proc_name[P_NAME_LEN];
} boot_image[NR_BOOT_PROCS] = {
    { -4, "HARDWARE" },
    { -3, "IDLE"     },
    { -2, "CLOCK"    },
    { -1, "SYSTEM"   },
    {  0, "PM"       },
    {  1, "FS"       },
    {  2, "TTY"      },
    {  3, "INIT"     },
};

/* =============================================================================
 * readline() -- read one line from the keyboard into buf (max len-1 + '\0')
 *
 * Echoes printable characters to VGA. Handles:
 *   '\b'  backspace -- erase previous character from buffer and screen
 *   '\n'  enter     -- terminate the line and return
 *   other           -- store in buffer and echo to screen
 *
 * Returns the number of characters read (not counting the null terminator).
 * =============================================================================
 */
static int readline(char *buf, int len)
{
    int n = 0;
    while (1) {
        char c = kbd_getchar();

        if (c == '\n') {
            buf[n] = '\0';
            vga_putchar('\n');
            return n;
        }

        if (c == '\b') {
            if (n > 0) {
                n--;
                vga_putchar('\b');   /* vga_putchar erases the character on \b */
            }
            continue;
        }

        /* Only store printable characters; silently drop others. */
        if (c >= 0x20 && c <= 0x7E && n < len - 1) {
            buf[n++] = c;
            vga_putchar(c);
        }
    }
}

/* =============================================================================
 * parse_cmd() -- split a line into argv[] tokens in-place
 *
 * Tokens are separated by spaces. Modifies buf by inserting null terminators.
 * Returns the number of tokens (argc). argv[] points into buf.
 * =============================================================================
 */
static int parse_cmd(char *buf, char *argv[], int max_args)
{
    int argc = 0;
    char *p  = buf;

    while (*p && argc < max_args) {
        /* Skip leading spaces. */
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;

        argv[argc++] = p;   /* start of token */

        /* Scan to end of token. */
        while (*p && *p != ' ' && *p != '\t') p++;

        /* Null-terminate the token. */
        if (*p) *p++ = '\0';
    }
    return argc;
}

/* =============================================================================
 * cmd_help() -- display the command reference screen
 * =============================================================================
 */
static void cmd_help(void)
{
    vga_writeln("");
    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_writeln("  +------------------------------------------------------------+");
    vga_writeln("  |          DugOS v1.0  --  Command Reference                 |");
    vga_writeln("  +------------------------------------------------------------+");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_writeln("");
    vga_writeln("  [ General ]");
    vga_writeln("    help                  Show this help screen");
    vga_writeln("    cls / clear           Clear the screen");
    vga_writeln("    pwd                   Print current directory");
    vga_writeln("    whoami                Show current user");
    vga_writeln("    shutdown              Shut down DugOS");
    vga_writeln("");
    vga_writeln("  [ File Operations ]");
    vga_writeln("    fwrite <file>         Create or overwrite a file");
    vga_writeln("    fread  <file>         Read and display a file");
    vga_writeln("    cat    <file>         Same as fread");
    vga_writeln("    fedit  <file>         Append content to a file");
    vga_writeln("    fdel   <file>         Delete a file");
    vga_writeln("    rename <old> <new>    Rename a file");
    vga_writeln("    copy   <src> <dst>    Copy a file");
    vga_writeln("");
    vga_writeln("  [ Directory Operations ]");
    vga_writeln("    mkdir  <dir>          Create a directory");
    vga_writeln("    cd     <dir>          Change directory (use '..' for parent)");
    vga_writeln("    rmdir  <dir>          Delete an empty directory");
    vga_writeln("    ls / dir              List current directory");
    vga_writeln("");
    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_writeln("  +------------------------------------------------------------+");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_writeln("");
}

/* =============================================================================
 * cmd_shutdown() -- graceful kernel shutdown
 *
 * Mirrors dug_os.c cmd_shutdown() (lines 559-583). Prints a shutdown sequence
 * (SIGKSTOP broadcast, flushing, masking IRQs), then restores BIOS PIC
 * defaults and powers off via QEMU's ACPI power-off port (0x604).
 * =============================================================================
 */
static void cmd_shutdown(void)
{
    vga_writeln("");
    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_writeln("  +----------------------------------------------------------+");
    vga_writeln("  |                   DugOS Shutdown                         |");
    vga_writeln("  +----------------------------------------------------------+");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_writeln("");
    vga_writeln("  [kernel] Sending SIGKSTOP to all system processes...");

    for (int i = 0; i < NR_BOOT_PROCS; i++) {
        if (boot_image[i].proc_nr != -4) {   /* skip HARDWARE */
            vga_write("  [kernel]   -> stopping ");
            vga_write(boot_image[i].proc_name);
            vga_write(" (proc ");
            vga_write_dec((uint32_t)(boot_image[i].proc_nr < 0
                          ? (uint32_t)(-boot_image[i].proc_nr)
                          : (uint32_t)boot_image[i].proc_nr));
            vga_writeln(")");
        }
    }

    vga_writeln("  [kernel] Flushing file system buffers ... OK");
    vga_writeln("  [kernel] Stopping clock task          ... OK");
    vga_writeln("  [kernel] Masking all hardware IRQs    ... OK");

    /* Restore BIOS PIC defaults (matches MINIX intr_init(0)). */
    pic_restore();
    vga_writeln("  [kernel] intr_init: restoring BIOS defaults ... OK");

    vga_writeln("");
    vga_writeln("  DugOS has shut down. Goodbye, dug.");
    vga_writeln("");

    /* Power off via QEMU ACPI PM1a control register.
     * 0x604 = QEMU power-off port (emulated ACPI controller).
     * If that doesn't work, 0xB004 works on older QEMU versions. */
    __asm__ __volatile__ ("cli");   /* disable interrupts: we are done */
    outb(0x604, 0x00);              /* QEMU >= 2.0 power off           */
    outb(0xB004, 0x00);             /* QEMU older fallback             */

    /* If QEMU power-off didn't work, halt forever. */
    for (;;) __asm__ __volatile__ ("hlt");
}

/* =============================================================================
 * shell_dispatch() -- match a parsed command and call the appropriate handler
 * =============================================================================
 */
static void shell_dispatch(char *argv[], int argc)
{
    const char *cmd = argv[0];
    const char *cwd = dir_getcwd();   /* used as the 'dir' argument for fs_* */

    /* ---- General commands ---- */
    if (kstrcmp(cmd, "help") == 0) {
        cmd_help();

    } else if (kstrcmp(cmd, "clear") == 0 || kstrcmp(cmd, "cls") == 0) {
        vga_clear();

    } else if (kstrcmp(cmd, "pwd") == 0) {
        vga_write("  "); vga_writeln(cwd);

    } else if (kstrcmp(cmd, "whoami") == 0) {
        vga_writeln("  dug");

    } else if (kstrcmp(cmd, "shutdown") == 0) {
        cmd_shutdown();

    /* ---- File operations (Phase C) ---- */
    } else if (kstrcmp(cmd, "fwrite") == 0) {
        fs_write(argc > 1 ? argv[1] : (char *)"", cwd);

    } else if (kstrcmp(cmd, "fread") == 0 || kstrcmp(cmd, "cat") == 0) {
        fs_read(argc > 1 ? argv[1] : (char *)"", cwd);

    } else if (kstrcmp(cmd, "fedit") == 0) {
        fs_edit(argc > 1 ? argv[1] : (char *)"", cwd);

    } else if (kstrcmp(cmd, "fdel") == 0) {
        fs_delete(argc > 1 ? argv[1] : (char *)"", cwd);

    } else if (kstrcmp(cmd, "rename") == 0) {
        fs_rename(argc > 1 ? argv[1] : (char *)"",
                  argc > 2 ? argv[2] : (char *)"",
                  cwd);

    } else if (kstrcmp(cmd, "copy") == 0) {
        fs_copy(argc > 1 ? argv[1] : (char *)"",
                argc > 2 ? argv[2] : (char *)"",
                cwd);

    /* ---- Directory operations (Phase D) ---- */
    } else if (kstrcmp(cmd, "mkdir") == 0) {
        dir_mkdir(argc > 1 ? argv[1] : (char *)"");

    } else if (kstrcmp(cmd, "cd") == 0) {
        dir_cd(argc > 1 ? argv[1] : (char *)"/");

    } else if (kstrcmp(cmd, "rmdir") == 0) {
        dir_rmdir(argc > 1 ? argv[1] : (char *)"");

    } else if (kstrcmp(cmd, "ls") == 0 || kstrcmp(cmd, "dir") == 0) {
        dir_ls();

    } else {
        /* Unknown command. */
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_write("  Unknown command: '"); vga_write(cmd);
        vga_writeln("'  (type 'help' for a list)");
        vga_set_color(VGA_WHITE, VGA_BLACK);
    }
}

/* =============================================================================
 * shell_run() -- interactive shell main loop (called from kmain, never returns)
 *
 * Displays the prompt "  dug@os:<cwd>$ ", reads one line, trims it, and
 * dispatches the command. Ignores empty lines. The only exit is 'shutdown'.
 *
 * Prompt format mirrors dug_os.c line 643:  "  dug@os:<cwd>$ "
 * =============================================================================
 */
void shell_run(void)
{
    static char  buf[CMD_LEN];
    char        *argv[MAX_ARGS];
    int          argc;

    /* Show a welcome hint on first entry (after boot messages have scrolled). */
    vga_writeln("");
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_writeln("  >> DugOS shell ready. Type 'help' for commands.");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_writeln("");

    while (1) {
        /* Display the prompt: "  dug@os:<cwd>$ " */
        vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
        vga_write("  dug@os:");
        vga_write(dir_getcwd());
        vga_write("$ ");
        vga_set_color(VGA_WHITE, VGA_BLACK);

        /* Read one full command line. */
        int len = readline(buf, CMD_LEN);
        if (len == 0) continue;   /* ignore empty lines */

        /* Parse into tokens and dispatch. */
        argc = parse_cmd(buf, argv, MAX_ARGS);
        if (argc == 0) continue;

        shell_dispatch(argv, argc);
        vga_writeln("");   /* blank line after each command's output */
    }
}
