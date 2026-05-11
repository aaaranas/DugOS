/* =============================================================================
 * pic.h -- DugOS 8259A Programmable Interrupt Controller interface
 *
 * PURPOSE:
 *   Declares the functions that initialize and control the dual 8259A PIC
 *   chip set found in all AT-compatible PCs. The PIC is the hardware bridge
 *   between physical IRQ lines (keyboard, timer, etc.) and the CPU's interrupt
 *   vector table (IDT).
 *
 * WHY WE NEED THIS:
 *   On reset, the BIOS programs the master PIC to map hardware IRQs 0-7 to
 *   CPU interrupt vectors 8-15. These OVERLAP with CPU exception vectors
 *   8-15 (double fault, coprocessor overrun, invalid TSS, etc.). If we leave
 *   this mapping in place, any keyboard press or timer tick fires a CPU
 *   exception handler instead of our IRQ handler -- causing an instant crash.
 *
 *   pic_init() remaps the PICs so IRQs 0-7 go to vectors 0x20-0x27 (32-39)
 *   and IRQs 8-15 go to vectors 0x28-0x2F (40-47). These are safely above the
 *   32 reserved exception vectors and match our IDT entries added in idt.c.
 *
 * REFERENCES:
 *   MINIX 3 source: reference/minix/kernel/i8259.c
 *   Intel 8259A datasheet
 *   OSDev Wiki -- 8259 PIC: https://wiki.osdev.org/8259_PIC
 * =============================================================================
 */

#ifndef DUGOS_PIC_H
#define DUGOS_PIC_H

#include <stdint.h>

/* =============================================================================
 * pic_init() -- remap PIC and enable hardware IRQs
 *
 * Sends ICW1-ICW4 to both the master and slave 8259A PICs to remap them:
 *   Master IRQs 0-7  --> CPU vectors 0x20-0x27 (32-39)
 *   Slave  IRQs 8-15 --> CPU vectors 0x28-0x2F (40-47)
 *
 * After remapping, all 16 IRQ lines are masked (disabled). Callers must
 * individually unmask lines they intend to use via pic_unmask().
 *
 * Call BEFORE enabling interrupts with sti. If sti is called before
 * pic_init(), hardware IRQs will still land on the wrong vectors and crash.
 * =============================================================================
 */
void pic_init(void);

/* =============================================================================
 * pic_restore() -- restore BIOS default PIC mapping and mask all IRQs
 *
 * Used during shutdown. Re-maps PICs to BIOS defaults (master at 0x08,
 * slave at 0x70) and masks all 16 IRQ lines. After this call, the machine
 * behaves as it did before the kernel ran, which is the cleanest state for
 * power-off or warm reboot.
 * =============================================================================
 */
void pic_restore(void);

/* =============================================================================
 * pic_eoi() -- send End-Of-Interrupt signal to the PIC(s)
 *
 * Parameter:
 *   irq -- the IRQ line (0-15) that was just serviced
 *
 * MUST be called at the end of every IRQ handler, or the PIC will never
 * deliver another interrupt on that line (it waits for EOI before unmasking).
 * For IRQs 8-15 (slave PIC), EOI must be sent to BOTH the slave and master.
 * =============================================================================
 */
void pic_eoi(uint8_t irq);

/* =============================================================================
 * pic_mask() / pic_unmask() -- enable or disable a single IRQ line
 *
 * Parameter:
 *   irq -- the IRQ line to control (0-15)
 *
 * pic_mask()   disables (masks)   the IRQ line so no interrupts are delivered.
 * pic_unmask() enables (unmasks) the IRQ line so interrupts are delivered.
 *
 * After pic_init(), ALL lines are masked. Call pic_unmask() for each IRQ
 * you have a handler for (e.g., IRQ0 timer, IRQ1 keyboard).
 * =============================================================================
 */
void pic_mask(uint8_t irq);
void pic_unmask(uint8_t irq);

#endif /* DUGOS_PIC_H */
