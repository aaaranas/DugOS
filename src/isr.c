/* =============================================================================
 * isr.c -- DugOS common CPU exception handler
 *
 * PURPOSE:
 *   Provides the C-level handler that is invoked whenever a CPU exception
 *   fires (vectors 0-31). It receives a full snapshot of the CPU register
 *   state (via struct regs in isr.h), displays a diagnostic message in red
 *   on the VGA screen, and halts the CPU.
 *
 * HOW CONTROL REACHES HERE:
 *   CPU exception fires
 *     --> isrN stub in isr_stubs.s  (pushes vector + error code)
 *         --> isr_common in isr_stubs.s  (saves all registers, calls this function)
 *             --> isr_common_handler() below  (displays message, halts)
 *
 * FUTURE EXTENSION:
 *   Once the shell and process model exist (Phase B.4+), non-fatal exceptions
 *   (e.g., vector 3 Breakpoint, vector 14 Page Fault) could be made
 *   recoverable by returning from this function instead of halting. The
 *   isr_common stub will then execute 'iret' to resume the interrupted code.
 *
 * REFERENCES:
 *   Intel SDM Vol.3, Table 6-1 (Protected-Mode Exceptions and Interrupts)
 *   struct regs definition: src/isr.h
 * =============================================================================
 */

#include "isr.h"   /* struct regs, isr_common_handler declaration */
#include "vga.h"   /* vga_write, vga_writeln, vga_set_color, etc. */

/* =============================================================================
 * exception_name() -- return a human-readable name for a CPU exception vector
 *
 * Parameter:
 *   vec -- the exception vector number (0-31)
 *
 * Returns:
 *   A pointer to a constant string describing the exception. Vectors that
 *   are officially reserved by Intel are labelled "Reserved/unknown".
 *
 * These names come from Intel SDM Vol.3, Table 6-1. They are displayed on
 * screen by isr_common_handler() to help identify the cause of a crash.
 * =============================================================================
 */
static const char *exception_name(uint32_t vec)
{
    switch (vec) {
        case 0:  return "Divide by zero";           /* DIV or IDIV with divisor 0 */
        case 1:  return "Debug";                    /* hardware debug breakpoint   */
        case 2:  return "Non-maskable interrupt";   /* NMI (hardware error signal) */
        case 3:  return "Breakpoint";               /* INT 3 software breakpoint   */
        case 4:  return "Overflow";                 /* INTO instruction + OF flag  */
        case 5:  return "Bound range exceeded";     /* BOUND instruction           */
        case 6:  return "Invalid opcode";           /* undefined or illegal opcode */
        case 7:  return "Device not available";     /* FPU not present or disabled */
        case 8:  return "Double fault";             /* exception while handling exception */
        case 9:  return "Coprocessor segment overrun"; /* legacy FPU (486 and older) */
        case 10: return "Invalid TSS";              /* bad task-state segment      */
        case 11: return "Segment not present";      /* segment with P=0 accessed   */
        case 12: return "Stack-segment fault";      /* SS segment fault or overflow*/
        case 13: return "General protection fault"; /* most common: privilege or   */
                                                    /*   bad segment usage         */
        case 14: return "Page fault";               /* virtual address not mapped  */
        case 16: return "x87 FPU exception";        /* floating-point arithmetic   */
        case 17: return "Alignment check";          /* misaligned memory access    */
        case 18: return "Machine check";            /* hardware-reported CPU error */
        case 19: return "SIMD FP exception";        /* SSE floating-point error    */
        default: return "Reserved/unknown";         /* vectors 15, 20-31           */
    }
}

/* =============================================================================
 * isr_common_handler() -- display exception details and halt the CPU
 *
 * Parameter:
 *   r -- pointer to the saved register frame on the kernel stack (see isr.h)
 *
 * This function is called by the isr_common assembly stub in isr_stubs.s
 * after all registers have been saved. It prints:
 *   - The exception vector number (r->vector)
 *   - The human-readable exception name from exception_name() above
 *   - The CPU-supplied error code in hexadecimal (r->err_code)
 *
 * After printing, the CPU is halted permanently with CLI + HLT in a loop.
 * CLI disables interrupts so nothing can wake the CPU after halting.
 * The loop handles the edge case where an NMI fires anyway.
 * =============================================================================
 */
void isr_common_handler(struct regs *r)
{
    /* Switch to red text on black background to make exceptions stand out. */
    vga_set_color(VGA_LIGHT_RED, VGA_BLACK);

    /* Print a blank line for visual separation from prior output. */
    vga_writeln("");

    /* Print the exception vector number and its name. */
    vga_write("  !! EXCEPTION ");
    vga_write_dec(r->vector);              /* e.g. "14" for page fault      */
    vga_write(" (");
    vga_write(exception_name(r->vector));  /* e.g. "Page fault"             */
    vga_write(")  err=0x");
    vga_write_hex(r->err_code);            /* CPU error code in hex         */
    vga_writeln("");

    /* Print the faulting instruction address (EIP) for debugging. */
    vga_write("  Faulting EIP=0x");
    vga_write_hex(r->eip);
    vga_writeln("");

    vga_writeln("  CPU halted. Restart QEMU to continue.");

    /* Permanently halt: disable interrupts then halt in an infinite loop.
     * The loop ensures that even a Non-Maskable Interrupt cannot escape. */
    for (;;) __asm__ __volatile__ ("cli; hlt");
}
