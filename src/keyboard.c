/* =============================================================================
 * keyboard.c -- DugOS PS/2 keyboard driver
 *
 * PURPOSE:
 *   Receives IRQ1 interrupts from the PS/2 keyboard controller, reads scan
 *   codes from the PS/2 data port (0x60), translates them to ASCII using a
 *   US QWERTY scan code table, and stores them in a circular ring buffer.
 *   The shell reads from the buffer via kbd_getchar() to form command lines.
 *
 * PS/2 SCAN CODE SET 1 (WHAT QEMU EMULATES):
 *   Each key press sends a "make code" (0x01-0x58 range). Each key release
 *   sends a "break code" which equals the make code with bit 7 set (OR 0x80).
 *
 *   We only need a subset of codes for a usable shell:
 *     0x01 = ESC                    0x0E = Backspace
 *     0x1C = Enter                  0x39 = Space
 *     0x02-0x0D = '1'-'=' row       0x10-0x1C = 'q'-Enter row
 *     0x1E-0x28 = 'a'-'\'' row      0x2C-0x35 = 'z'-'/' row
 *     0x2A, 0x36 = L/R Shift        0x3A = Caps Lock
 *
 * RING BUFFER:
 *   A power-of-2 sized circular buffer (256 bytes) with head (write) and tail
 *   (read) indices. The keyboard ISR writes; kbd_getchar() reads. The buffer
 *   is interrupt-driven: only the ISR writes, only user code reads, so no
 *   locking is needed on a single-CPU kernel.
 *
 * REFERENCES:
 *   MINIX 3 source: reference/minix/drivers/tty/keyboard.c
 *   PS/2 scan code table: reference/minix/drivers/tty/keymaps/us-std.src
 *   OSDev Wiki -- PS/2 Keyboard: https://wiki.osdev.org/PS2_Keyboard
 * =============================================================================
 */

#include "keyboard.h"
#include "isr.h"     /* irq_register() */
#include "pic.h"     /* pic_unmask()   */
#include "port.h"    /* inb()          */

/* PS/2 controller I/O ports */
#define KBD_DATA_PORT   0x60   /* data register: read scan codes here     */
#define KBD_STATUS_PORT 0x64   /* status register: check controller ready */

/* Ring buffer parameters */
#define KBD_BUF_SIZE 256   /* must be a power of 2 for the % trick to work */

/* =============================================================================
 * Internal state
 * =============================================================================
 */

/* Ring buffer: head = next write position; tail = next read position.
 * Both are indices into kbd_buf[]. Buffer is full if (head+1)%SIZE == tail;
 * buffer is empty if head == tail. */
static volatile char     kbd_buf[KBD_BUF_SIZE];
static volatile uint32_t kbd_head;   /* written by ISR (interrupt context)   */
static volatile uint32_t kbd_tail;   /* read by shell (normal context)        */

/* Modifier key state: updated on every make/break code. */
static volatile uint8_t shift_held;    /* 1 if Left or Right Shift is pressed */
static volatile uint8_t caps_lock_on;  /* 1 if Caps Lock is active            */

/* =============================================================================
 * US QWERTY scan code tables (PS/2 Set 1 make codes 0x00-0x53)
 *
 * Two tables: one for unshifted keys, one for shifted. Index = scan code.
 * Entries are 0 for keys that produce no printable character (modifiers,
 * function keys, arrow keys). The ISR uses these to translate scan codes.
 *
 * Source: reference/minix/drivers/tty/keymaps/us-std.src (adapted for
 * freestanding use without the MINIX keymap infrastructure).
 * =============================================================================
 */
static const char scancode_normal[128] = {
    /*00*/  0,   27,  '1', '2', '3', '4', '5', '6', '7', '8',
    /*0A*/  '9', '0', '-', '=', '\b', '\t',
    /*10*/  'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p',
    /*1A*/  '[', ']', '\n', 0,
    /*1E*/  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',
    /*28*/  '\'', '`', 0,
    /*2B*/  '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',
    /*36*/  0,  '*', 0,  ' ',
    /*3A*/  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   /* CapsLock, F1-F9 */
    /*44*/  0,                               /* F10              */
    /*45*/  0, 0,                            /* NumLock, ScrollLk*/
    /*47*/  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   /* Keypad 7-/ */
    /*51*/  0, 0, 0,                         /* Keypad . Del - */
    /*54 to 7F*/ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                 0, 0, 0, 0, 0, 0, 0, 0
};

static const char scancode_shift[128] = {
    /*00*/  0,   27,  '!', '@', '#', '$', '%', '^', '&', '*',
    /*0A*/  '(', ')', '_', '+', '\b', '\t',
    /*10*/  'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P',
    /*1A*/  '{', '}', '\n', 0,
    /*1E*/  'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',
    /*28*/  '"', '~', 0,
    /*2B*/  '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?',
    /*36*/  0,  '*', 0,  ' ',
    /*3A*/  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /*44*/  0,
    /*45*/  0, 0,
    /*47*/  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /*51*/  0, 0, 0,
    /*54 to 7F*/ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                 0, 0, 0, 0, 0, 0, 0, 0
};

/* =============================================================================
 * kbd_put() -- write one character into the ring buffer (called from ISR)
 *
 * Advances head by 1 (modulo buffer size). If the buffer is full (would make
 * head equal tail), the character is silently dropped. This prevents a stalled
 * shell from locking up the system -- at worst a few keystrokes are lost.
 * =============================================================================
 */
static void kbd_put(char c)
{
    uint32_t next_head = (kbd_head + 1) % KBD_BUF_SIZE;
    if (next_head != kbd_tail) {       /* only write if there is space */
        kbd_buf[kbd_head] = c;
        kbd_head = next_head;
    }
}

/* =============================================================================
 * kbd_isr() -- keyboard interrupt handler (called from irq_dispatch on IRQ1)
 *
 * Reads one scan code from the PS/2 data port. Break codes (bit 7 set) are
 * used only to clear Shift state. Make codes are translated to ASCII using
 * the scan code tables above, modified by current Shift/Caps state.
 * =============================================================================
 */
static void kbd_isr(void)
{
    uint8_t sc = inb(KBD_DATA_PORT);   /* read the scan code from the PS/2 port */

    if (sc & 0x80) {
        /* Key RELEASE (break code): clear Shift if it was released. */
        uint8_t make = sc & 0x7F;        /* strip the break bit */
        if (make == 0x2A || make == 0x36)
            shift_held = 0;              /* Left or Right Shift released */
        return;                          /* ignore all other break codes */
    }

    /* Key PRESS (make code) -- handle modifiers and printable keys. */
    switch (sc) {
        case 0x2A: case 0x36:           /* Left Shift / Right Shift pressed */
            shift_held = 1;
            return;

        case 0x3A:                      /* Caps Lock toggle */
            caps_lock_on ^= 1;
            return;

        default: {
            /* Translate the scan code to ASCII using the appropriate table. */
            char c;
            if (shift_held) {
                c = scancode_shift[sc];
            } else {
                c = scancode_normal[sc];
            }

            if (c == 0) return;         /* no printable representation */

            /* Apply Caps Lock: flip case for letters only.
             * If CapsLock is on and Shift is NOT held (or vice versa), uppercase.
             * This mirrors standard keyboard behaviour. */
            if (c >= 'a' && c <= 'z') {
                if (caps_lock_on ^ shift_held)
                    c = (char)(c - 'a' + 'A');  /* convert to uppercase */
            } else if (c >= 'A' && c <= 'Z') {
                if (caps_lock_on ^ shift_held)
                    c = (char)(c - 'A' + 'a');  /* convert to lowercase */
            }

            kbd_put(c);  /* place the translated character in the ring buffer */
        }
    }
}

/* =============================================================================
 * kbd_init() -- register IRQ1 handler and unmask the keyboard IRQ line
 *
 * After this returns, every key press will fire IRQ1, run kbd_isr(), and
 * add the resulting ASCII character to kbd_buf[]. The shell then reads from
 * the buffer via kbd_getchar().
 *
 * kbd_init() must be called AFTER pic_init() (PIC must be remapped) and
 * BEFORE sti (the handler is installed before interrupts are enabled).
 * =============================================================================
 */
void kbd_init(void)
{
    irq_register(1, kbd_isr);   /* IRQ1 = PS/2 keyboard */
    pic_unmask(1);               /* enable IRQ1 on the master PIC */
}

/* =============================================================================
 * kbd_has_char() -- non-blocking check whether the buffer has a character
 * =============================================================================
 */
int kbd_has_char(void)
{
    return kbd_head != kbd_tail;
}

/* =============================================================================
 * kbd_getchar() -- blocking read: halt until a key is available then return it
 *
 * The 'hlt' instruction pauses the CPU until the next interrupt fires.
 * When a key is pressed, IRQ1 fires, kbd_isr() writes to the buffer, and
 * hlt returns. We then re-check the buffer in the while-loop and return.
 *
 * Interrupts must already be enabled (sti called) for hlt to wake up.
 * =============================================================================
 */
char kbd_getchar(void)
{
    /* Halt until the IRQ1 handler puts something in the ring buffer. */
    while (!kbd_has_char()) {
        __asm__ __volatile__ ("hlt");
    }

    /* Consume the next character from the tail of the ring buffer. */
    char c = kbd_buf[kbd_tail];
    kbd_tail = (kbd_tail + 1) % KBD_BUF_SIZE;
    return c;
}
