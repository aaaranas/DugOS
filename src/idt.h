/* =============================================================================
 * idt.h -- DugOS Interrupt Descriptor Table interface
 *
 * PURPOSE:
 *   Declares the public interface for the IDT subsystem. The Interrupt
 *   Descriptor Table tells the CPU where to jump when an interrupt or
 *   CPU exception occurs.
 *
 * WHAT IS THE IDT?
 *   In 32-bit protected mode the CPU has 256 possible interrupt vectors
 *   (0-255). Each vector maps to an entry in the IDT. When an interrupt
 *   fires, the CPU automatically:
 *     1. Pushes the current CS, EIP, EFLAGS (and error code if applicable)
 *        onto the kernel stack.
 *     2. Looks up the handler address in the IDT entry for that vector.
 *     3. Jumps to the handler.
 *
 * VECTOR RANGES:
 *   Vectors  0-31  -- CPU exceptions (divide-by-zero, page fault, GPF, ...)
 *   Vectors 32-47  -- Hardware IRQs after 8259A PIC remap (Phase B.2)
 *   Vectors 48-255 -- Available for software interrupts / syscalls
 *
 * USAGE:
 *   Call idt_init() once from kmain() in main.c after gdt_init(). After it
 *   returns, all 32 CPU exception vectors are handled. Hardware IRQ vectors
 *   are added in Phase B.2 (intr_init).
 * =============================================================================
 */

#ifndef DUGOS_IDT_H
#define DUGOS_IDT_H

#include <stdint.h>  /* uint8_t, uint16_t, uint32_t */

/* =============================================================================
 * idt_init() -- fill the 256-entry IDT and load it into the CPU
 *
 * Installs ISR stubs (from isr_stubs.s) for CPU exception vectors 0-31.
 * All 256 entries are zeroed first so unused vectors stay null -- any
 * spurious interrupt on an unhandled vector will trigger a double fault,
 * which IS handled (vector 8) and will print a diagnostic message.
 *
 * Calls idt_flush() (in isr_stubs.s) to execute the lidt instruction.
 * =============================================================================
 */
void idt_init(void);

#endif /* DUGOS_IDT_H */
