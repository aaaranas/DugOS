/* =============================================================================
 * shell.h -- DugOS interactive command shell interface (Phase B.4)
 *
 * PURPOSE:
 *   Declares the entry point for the DugOS interactive shell. The shell reads
 *   one command line at a time from the keyboard (via kbd_getchar), parses it
 *   into a command name and arguments, and dispatches to the appropriate
 *   handler. It runs in an infinite loop and is the last thing called from
 *   kmain() in main.c.
 *
 * COMMAND SET:
 *   General:     help, clear/cls, pwd, whoami, shutdown
 *   File ops:    fwrite, fread/cat, fedit, fdel, rename, copy
 *   Directory:   mkdir, cd, rmdir, ls/dir
 *
 * PROMPT FORMAT:
 *   "  dug@os:<cwd>$ "  (matches dug_os.c prototype, line 643)
 *   Example: "  dug@os:/$ " or "  dug@os:/home$ "
 *
 * REFERENCES:
 *   dug_os.c lines 589-654 -- behavioral prototype (shell_dispatch, shell_run)
 *   TASK_HANDOFF.md -- Phase B.4 implementation notes
 * =============================================================================
 */

#ifndef DUGOS_SHELL_H
#define DUGOS_SHELL_H

/* =============================================================================
 * shell_run() -- enter the interactive command shell loop (never returns)
 *
 * Displays the prompt, reads a line of input character by character via
 * kbd_getchar(), and dispatches the command. Loops forever. The only way to
 * exit is the 'shutdown' command, which powers off the machine.
 * =============================================================================
 */
void shell_run(void);

#endif /* DUGOS_SHELL_H */
