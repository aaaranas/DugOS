/* =============================================================================
 * isr.h -- DugOS interrupt/exception handler interface
 *
 * PURPOSE:
 *   Defines the 'struct regs' layout that captures the full CPU register
 *   state at the moment an exception fires, and declares the common C
 *   handler function that receives it.
 *
 * HOW THE REGISTER STATE IS CAPTURED:
 *   When a CPU exception occurs, the hardware and our ISR stubs cooperate
 *   to push a full register snapshot onto the kernel stack:
 *
 *     (1) CPU pushes automatically: SS, useresp, EFLAGS, CS, EIP
 *         (and for some vectors, an error code before EIP)
 *     (2) ISR stub (isr_stubs.s) pushes: vector number, dummy error code
 *         (if the CPU did not push one), then executes 'pusha' (all GPRs)
 *         and pushes the segment registers DS, ES, FS, GS.
 *
 *   The combined stack frame exactly matches 'struct regs' below when read
 *   from the lowest address (ESP) upward. A pointer to this struct is
 *   passed to isr_common_handler() as its only argument.
 *
 * CRITICAL COUPLING:
 *   The field ORDER in struct regs must exactly match the PUSH ORDER in
 *   isr_stubs.s:isr_common. If either is changed, the other must be
 *   updated simultaneously -- a mismatch produces wrong register values
 *   with no compile-time error.
 * =============================================================================
 */

#ifndef DUGOS_ISR_H
#define DUGOS_ISR_H

#include <stdint.h>  /* uint32_t */

/* =============================================================================
 * struct regs -- snapshot of the CPU register state at exception entry
 *
 * Field order (lowest address = first field = what ESP points to):
 *
 *   gs, fs, es, ds    -- segment registers, pushed by isr_common in isr_stubs.s
 *   edi...eax         -- general-purpose registers, pushed by 'pusha' instruction
 *   vector            -- exception/interrupt vector number, pushed by the stub
 *   err_code          -- CPU error code (real for vectors 8,10-14,17,30;
 *                        dummy 0 for all others, pushed by the ISR_NOERR macro)
 *   eip, cs, eflags   -- return address and flags, pushed by the CPU automatically
 *   useresp, ss       -- user-mode stack pointer and segment (pushed by CPU only
 *                        when a privilege level change occurs; ring 0->ring 0
 *                        exceptions may not have these -- read carefully before use)
 * =============================================================================
 */
struct regs {
    /* Segment registers saved by isr_common (pushed in ds, es, fs, gs order,
     * so gs is at the lowest address after all four pushes). */
    uint32_t gs, fs, es, ds;

    /* General-purpose registers saved by the 'pusha' instruction.
     * pusha pushes: EAX, ECX, EDX, EBX, ESP(original), EBP, ESI, EDI.
     * ESP is at the lowest address after pusha, so edi is first in the struct. */
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;

    /* Exception vector number and error code, pushed by the ISR stub. */
    uint32_t vector;    /* which CPU exception fired (0-31 for Phase B.1) */
    uint32_t err_code;  /* CPU-supplied error code, or 0 if not applicable */

    /* These fields are pushed automatically by the CPU when the exception fires.
     * EIP is the address of the instruction that caused the fault. */
    uint32_t eip;      /* instruction pointer at the time of the fault   */
    uint32_t cs;       /* code segment at the time of the fault           */
    uint32_t eflags;   /* CPU flags at the time of the fault              */
    uint32_t useresp;  /* user-mode ESP (only valid on privilege change)  */
    uint32_t ss;       /* user-mode SS  (only valid on privilege change)  */
};

/* =============================================================================
 * isr_common_handler() -- C handler called from the assembly common stub
 *
 * Parameter:
 *   r -- pointer to the struct regs snapshot on the kernel stack
 *
 * This function is called by isr_common in isr_stubs.s after all registers
 * have been saved. It uses the VGA driver to display the exception vector,
 * name, and error code, then halts the CPU. In a future phase this could
 * be extended to handle recoverable exceptions (e.g., page faults for
 * demand paging) by returning normally and letting isr_common execute iret.
 * =============================================================================
 */
void isr_common_handler(struct regs *r);

/* =============================================================================
 * IRQ dispatcher interface (Phase B.2)
 *
 * irq_register() -- register a C function as the handler for one IRQ line
 *
 * Parameters:
 *   irq     -- hardware IRQ number (0-15). IRQ1 = keyboard, IRQ0 = timer.
 *   handler -- pointer to a void function(void) that services the IRQ.
 *              Called from irq_dispatch() with interrupts disabled.
 *              The handler must NOT send EOI -- irq_dispatch does that.
 *
 * irq_dispatch() -- called from the NASM IRQ stubs in isr_stubs.s
 *
 * Parameter:
 *   irq -- hardware IRQ number (0-15) passed from the NASM stub
 *
 * Calls the registered handler (if any) and sends End-Of-Interrupt to the
 * 8259A PIC. This is the central dispatch point for all hardware interrupts.
 * =============================================================================
 */
void irq_register(uint8_t irq, void (*handler)(void));
void irq_dispatch(uint32_t irq);

#endif /* DUGOS_ISR_H */
