/* =============================================================================
 * vga.h -- DugOS VGA text-mode driver interface
 *
 * PURPOSE:
 *   Declares all functions and constants needed to write text to the screen
 *   in VGA text mode. The VGA text buffer is a region of physical memory
 *   at address 0xB8000 that the graphics hardware reads and displays.
 *
 * HOW VGA TEXT MODE WORKS:
 *   The buffer holds 80 x 25 = 2000 cells. Each cell is 2 bytes:
 *     - Low byte:  ASCII character code to display
 *     - High byte: colour attribute (foreground in bits 0-3, background in 4-6)
 *   Writing directly to 0xB8000 is instant -- no OS support needed.
 *
 * USAGE:
 *   Call vga_init() once at boot. Then use vga_write() / vga_writeln() for
 *   all text output. Use vga_set_color() to change the text colour.
 * =============================================================================
 */

#ifndef DUGOS_VGA_H
#define DUGOS_VGA_H

#include <stddef.h>   /* size_t                */
#include <stdint.h>   /* uint8_t, uint16_t ... */

/* =============================================================================
 * vga_color -- colour indices for VGA text mode
 *
 * These 4-bit values are used as foreground (bits 0-3) or background (bits
 * 4-6) in the colour attribute byte of each VGA cell. The hardware maps each
 * number to a fixed colour from the default CGA palette.
 * =============================================================================
 */
enum vga_color {
    VGA_BLACK        = 0,   /* dark colours (bits 3=0) */
    VGA_BLUE         = 1,
    VGA_GREEN        = 2,
    VGA_CYAN         = 3,
    VGA_RED          = 4,
    VGA_MAGENTA      = 5,
    VGA_BROWN        = 6,
    VGA_LIGHT_GREY   = 7,
    VGA_DARK_GREY    = 8,   /* bright colours (bits 3=1) */
    VGA_LIGHT_BLUE   = 9,
    VGA_LIGHT_GREEN  = 10,
    VGA_LIGHT_CYAN   = 11,
    VGA_LIGHT_RED    = 12,
    VGA_LIGHT_MAGENTA= 13,
    VGA_YELLOW       = 14,
    VGA_WHITE        = 15
};

/* -- Initialise the VGA driver: set default colour and clear the screen. */
void vga_init(void);

/* -- Fill the entire 80x25 display with spaces using the current colour. */
void vga_clear(void);

/* -- Set the foreground and background colour for all subsequent output.
 *    fg and bg must be values from enum vga_color above. */
void vga_set_color(uint8_t fg, uint8_t bg);

/* -- Write a single character to the current cursor position.
 *    '\n' moves to the next line; reaching column 80 also wraps.
 *    Scrolls the display up one line when row 25 is reached. */
void vga_putchar(char c);

/* -- Write a null-terminated string without a trailing newline. */
void vga_write(const char *s);

/* -- Write a null-terminated string followed by a newline character. */
void vga_writeln(const char *s);

/* -- Write an unsigned 32-bit integer in decimal (no leading zeros). */
void vga_write_dec(uint32_t n);

/* -- Write an unsigned 32-bit integer in hexadecimal (no leading zeros,
 *    no "0x" prefix). Used by the exception handler to show error codes. */
void vga_write_hex(uint32_t n);

#endif /* DUGOS_VGA_H */
