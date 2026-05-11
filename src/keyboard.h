/* =============================================================================
 * keyboard.h -- DugOS PS/2 keyboard driver interface
 *
 * PURPOSE:
 *   Declares the interface for the PS/2 keyboard driver. The driver receives
 *   hardware interrupts on IRQ1 (vector 33), reads the scan code from the
 *   PS/2 data port (0x60), translates it to ASCII using a US QWERTY table,
 *   and stores the result in an internal ring buffer. The shell reads from
 *   this buffer via kbd_getchar() to build command lines.
 *
 * HOW IT FITS INTO THE SYSTEM:
 *   kbd_init() registers the keyboard interrupt handler with the IRQ dispatcher
 *   (irq_register from isr.h) and unmasks IRQ1 on the PIC (pic_unmask from
 *   pic.h). After kbd_init(), every key press produces an IRQ1 which calls
 *   the handler, which writes an ASCII character to the ring buffer.
 *
 *   The shell calls kbd_getchar(), which halts the CPU (hlt) until the buffer
 *   is non-empty. This is power-efficient because hlt pauses the CPU until
 *   the next interrupt (the keyboard IRQ), rather than busy-looping.
 *
 * REFERENCES:
 *   MINIX 3 source: reference/minix/drivers/tty/keyboard.c
 *   PS/2 scan code set 1 (AT-compatible, what QEMU emulates)
 *   OSDev Wiki -- PS/2 Keyboard: https://wiki.osdev.org/PS2_Keyboard
 * =============================================================================
 */

#ifndef DUGOS_KEYBOARD_H
#define DUGOS_KEYBOARD_H

#include <stdint.h>

/* =============================================================================
 * kbd_init() -- initialize the PS/2 keyboard driver
 *
 * Registers the keyboard IRQ handler with irq_register() and enables IRQ1
 * on the 8259A PIC via pic_unmask(). After this call, key presses produce
 * characters in the internal ring buffer.
 *
 * Must be called AFTER pic_init() (the PIC must be remapped first) and AFTER
 * sti has been executed (or the IRQ will never fire).
 * =============================================================================
 */
void kbd_init(void);

/* =============================================================================
 * kbd_has_char() -- check if the ring buffer has at least one character
 *
 * Returns:
 *   1 if a character is available, 0 if the buffer is empty.
 *
 * Non-blocking: does not halt the CPU. Use this to poll the buffer.
 * =============================================================================
 */
int kbd_has_char(void);

/* =============================================================================
 * kbd_getchar() -- read one character from the keyboard buffer (blocking)
 *
 * Returns:
 *   The next ASCII character from the ring buffer.
 *
 * If no character is available, halts the CPU (hlt) and waits for the next
 * keyboard interrupt. This is the primary input function used by the shell.
 * =============================================================================
 */
char kbd_getchar(void);

#endif /* DUGOS_KEYBOARD_H */
