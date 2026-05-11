/* =============================================================================
 * vga.c -- DugOS VGA text-mode driver implementation
 *
 * PURPOSE:
 *   Provides all screen output for DugOS by writing directly to the VGA text
 *   buffer in physical memory. No operating system services or standard
 *   library functions (printf, etc.) are available in a freestanding kernel,
 *   so this driver replaces them entirely.
 *
 * VGA TEXT BUFFER LAYOUT:
 *   Physical address : 0xB8000
 *   Size             : 80 columns x 25 rows = 2000 cells
 *   Cell format      : 16 bits per cell
 *                       Bits 15-8 = colour attribute byte
 *                                     bits 7-4 = background colour (0-6)
 *                                     bits 3-0 = foreground colour (0-15)
 *                       Bits  7-0 = ASCII character code
 *
 * SCROLLING:
 *   When the cursor reaches row 25, every row is shifted up by one (row 1
 *   becomes row 0, etc.) and the last row is cleared. This happens inside
 *   scroll_if_needed(), which is called by vga_putchar() automatically.
 *
 * REFERENCES:
 *   OSDev Wiki -- VGA Hardware: https://wiki.osdev.org/VGA_Hardware
 * =============================================================================
 */

#include "vga.h"

/* Physical address of the VGA text buffer. Cast to a volatile pointer so the
 * compiler never caches or optimises away writes -- the hardware reads this
 * memory directly and must see every update immediately. */
#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define VGA_MEM    ((volatile uint16_t *) 0xB8000)

/* Module-level state: current cursor position and active colour attribute. */
static size_t  row;    /* current cursor row    (0 = top)  */
static size_t  col;    /* current cursor column (0 = left) */
static uint8_t color;  /* packed colour attribute byte     */

/* =============================================================================
 * make_entry() -- pack a character and colour into a 16-bit VGA cell value
 *
 * Parameters:
 *   c    -- ASCII character to display
 *   attr -- colour attribute byte (foreground | background << 4)
 *
 * Returns:
 *   A 16-bit value ready to be written into the VGA buffer. The character
 *   occupies the low byte and the colour occupies the high byte.
 * =============================================================================
 */
static uint16_t make_entry(char c, uint8_t attr)
{
    /* Cast c to unsigned char first to prevent sign-extension when widening
     * to uint16_t (e.g. if c were 0x80 or higher on a signed-char platform). */
    return (uint16_t) (unsigned char) c | ((uint16_t) attr << 8);
}

/* =============================================================================
 * vga_set_color() -- change the foreground and background colour
 *
 * Parameters:
 *   fg -- foreground colour (text colour), use enum vga_color values
 *   bg -- background colour, use enum vga_color values
 *
 * The colour is packed into a single byte: bg occupies bits 6-4 and fg
 * occupies bits 3-0. This byte is stored in the global 'color' variable
 * and used by make_entry() for every subsequent character written.
 * =============================================================================
 */
void vga_set_color(uint8_t fg, uint8_t bg)
{
    color = (uint8_t) (fg | (bg << 4));  /* pack: high nibble = bg, low = fg */
}

/* =============================================================================
 * vga_clear() -- fill the entire screen with spaces
 *
 * Writes a space character with the current colour to every cell in the
 * 80x25 VGA buffer, then resets the cursor to the top-left corner (row=0,
 * col=0). Called by vga_init() at boot and can be called again for cls.
 * =============================================================================
 */
void vga_clear(void)
{
    for (size_t y = 0; y < VGA_HEIGHT; y++)
        for (size_t x = 0; x < VGA_WIDTH; x++)
            VGA_MEM[y * VGA_WIDTH + x] = make_entry(' ', color);  /* space with current colour */

    row = 0;  /* reset cursor to top-left */
    col = 0;
}

/* =============================================================================
 * vga_init() -- initialise the VGA driver at kernel startup
 *
 * Sets the default text colour to light-grey on black (the classic terminal
 * look) and clears the screen. Must be called once before any other VGA
 * function. Called from kmain() in main.c as the very first boot stage.
 * =============================================================================
 */
void vga_init(void)
{
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);  /* default: grey text on black */
    vga_clear();                               /* blank the screen             */
}

/* =============================================================================
 * scroll_if_needed() -- scroll the display up one line if the cursor is past
 *                       the last row
 *
 * Called internally by vga_putchar() after every newline or column wrap.
 * If 'row' is still within the 25-row display, this function does nothing.
 * If 'row' has gone past row 24 (the last row), every line is shifted up:
 *   - Row 1 is copied to row 0, row 2 to row 1, ..., row 24 to row 23.
 *   - Row 24 (the new last row) is filled with spaces.
 *   - The cursor is placed on row 24, ready for the next line of text.
 * =============================================================================
 */
static void scroll_if_needed(void)
{
    if (row < VGA_HEIGHT) return;  /* still within the visible area -- nothing to do */

    /* Shift every row up by one: copy row y into row y-1. */
    for (size_t y = 1; y < VGA_HEIGHT; y++)
        for (size_t x = 0; x < VGA_WIDTH; x++)
            VGA_MEM[(y - 1) * VGA_WIDTH + x] = VGA_MEM[y * VGA_WIDTH + x];

    /* Clear the last row (it now contains a duplicate of the row above it). */
    for (size_t x = 0; x < VGA_WIDTH; x++)
        VGA_MEM[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = make_entry(' ', color);

    row = VGA_HEIGHT - 1;  /* keep cursor on the last (now blank) row */
}

/* =============================================================================
 * vga_putchar() -- write a single character at the current cursor position
 *
 * Parameters:
 *   c -- the ASCII character to write (or '\n' for a newline)
 *
 * Handles two special cases:
 *   '\n' (newline)  -- move to column 0 of the next row
 *   column overflow -- when col reaches 80, wrap to the next row
 *
 * After any row change, scroll_if_needed() is called to scroll the display
 * if the cursor has gone past the bottom of the screen.
 * =============================================================================
 */
void vga_putchar(char c)
{
    if (c == '\n') {
        /* Newline: move cursor to the start of the next row. */
        col = 0;
        row++;
        scroll_if_needed();
        return;
    }

    /* Write the character into the VGA buffer at the current cursor position.
     * The cell index is: row * 80 + col (row-major layout). */
    VGA_MEM[row * VGA_WIDTH + col] = make_entry(c, color);

    /* Advance the cursor one column to the right. */
    col++;

    /* If we've gone past the last column, wrap to the next row. */
    if (col >= VGA_WIDTH) {
        col = 0;
        row++;
        scroll_if_needed();
    }
}

/* =============================================================================
 * vga_write() -- write a null-terminated string without a trailing newline
 *
 * Parameters:
 *   s -- pointer to the string to display (must be null-terminated)
 *
 * Iterates through the string one character at a time, calling vga_putchar()
 * for each. Stops when the null terminator '\0' is reached.
 * =============================================================================
 */
void vga_write(const char *s)
{
    while (*s) vga_putchar(*s++);  /* write each char until '\0' */
}

/* =============================================================================
 * vga_writeln() -- write a string followed by a newline
 *
 * Parameters:
 *   s -- pointer to the string to display (must be null-terminated)
 *
 * Convenience wrapper: calls vga_write() then adds a '\n'. Most screen
 * output in the kernel uses this function to keep lines tidy.
 * =============================================================================
 */
void vga_writeln(const char *s)
{
    vga_write(s);
    vga_putchar('\n');
}

/* =============================================================================
 * vga_write_dec() -- write an unsigned 32-bit integer in decimal
 *
 * Parameters:
 *   n -- the number to display
 *
 * Converts n to its decimal digit string by repeatedly dividing by 10 and
 * storing remainders in a local buffer. Because the remainders come out in
 * reverse order (least significant digit first), the buffer is printed
 * backwards. No leading zeros are printed.
 *
 * Used by isr_common_handler() to display the exception vector number.
 * =============================================================================
 */
void vga_write_dec(uint32_t n)
{
    char buf[11];  /* max 10 decimal digits for 2^32, plus null (unused here) */
    int  i = 0;

    /* Special case: zero would produce no digits in the loop below. */
    if (n == 0) { vga_putchar('0'); return; }

    /* Extract digits from least significant to most significant. */
    while (n > 0) {
        buf[i++] = (char) ('0' + (n % 10));  /* remainder gives the next digit */
        n /= 10;                              /* move to the next digit position */
    }

    /* Print the digits in reverse order (most significant first). */
    while (i > 0) vga_putchar(buf[--i]);
}

/* =============================================================================
 * vga_write_hex() -- write an unsigned 32-bit integer in hexadecimal
 *
 * Parameters:
 *   n -- the number to display (leading zeros are suppressed)
 *
 * Extracts each 4-bit nibble from most significant to least significant by
 * shifting n right in steps of 4. Leading zero nibbles are skipped until
 * the first non-zero nibble is found ('started' flag). Always prints at
 * least one digit (the final shift == 0 case forces output of '0').
 *
 * Used by isr_common_handler() to display the CPU error code in hex.
 * No "0x" prefix is printed -- callers add that themselves if needed.
 * =============================================================================
 */
void vga_write_hex(uint32_t n)
{
    static const char hex[] = "0123456789ABCDEF";  /* hex digit lookup table */
    int started = 0;  /* flag: have we printed any non-zero digit yet? */

    /* Iterate through all 8 nibbles (32 bits / 4 bits per nibble). */
    for (int shift = 28; shift >= 0; shift -= 4) {
        char c = hex[(n >> shift) & 0xF];  /* extract nibble and convert to char */

        /* Skip leading zeros, but always print the last nibble (shift == 0). */
        if (c != '0' || started || shift == 0) {
            vga_putchar(c);
            started = 1;  /* we've started printing, no more leading-zero skipping */
        }
    }
}
