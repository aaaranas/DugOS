/* DugOS -- common exception handler.
 *
 * Entered from the per-vector stubs in isr.s. Prints the offending
 * vector + error code and halts. Once we have a shell (Phase B.4)
 * this can be made recoverable for non-fatal exceptions.
 */

#include "isr.h"
#include "vga.h"

static const char *exception_name(uint32_t vec)
{
    switch (vec) {
        case 0:  return "Divide by zero";
        case 1:  return "Debug";
        case 2:  return "Non-maskable interrupt";
        case 3:  return "Breakpoint";
        case 4:  return "Overflow";
        case 5:  return "Bound range exceeded";
        case 6:  return "Invalid opcode";
        case 7:  return "Device not available";
        case 8:  return "Double fault";
        case 9:  return "Coprocessor segment overrun";
        case 10: return "Invalid TSS";
        case 11: return "Segment not present";
        case 12: return "Stack-segment fault";
        case 13: return "General protection fault";
        case 14: return "Page fault";
        case 16: return "x87 FPU exception";
        case 17: return "Alignment check";
        case 18: return "Machine check";
        case 19: return "SIMD FP exception";
        default: return "Reserved/unknown";
    }
}

void isr_common_handler(struct regs *r)
{
    vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
    vga_writeln("");
    vga_write("  !! EXCEPTION ");
    vga_write_dec(r->vector);
    vga_write(" (");
    vga_write(exception_name(r->vector));
    vga_write(")  err=0x");
    vga_write_hex(r->err_code);
    vga_writeln("");
    vga_writeln("  CPU halted.");

    for (;;) __asm__ __volatile__ ("cli; hlt");
}
