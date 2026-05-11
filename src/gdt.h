/* =============================================================================
 * gdt.h -- DugOS Global Descriptor Table interface
 *
 * PURPOSE:
 *   Declares the public interface for the GDT subsystem. The Global
 *   Descriptor Table is a CPU data structure that defines memory segments
 *   and their access permissions in 32-bit protected mode.
 *
 * WHY WE NEED A GDT:
 *   When GRUB boots the kernel it installs a minimal temporary GDT. We
 *   replace it with our own 5-entry table so we have full control over
 *   segment selectors, privilege levels (ring 0 kernel / ring 3 user), and
 *   descriptor flags. Without this, we cannot safely set up the IDT or
 *   switch to user mode later.
 *
 * USAGE:
 *   Call gdt_init() once from kmain() in main.c. After it returns, the
 *   CPU's CS register points to our kernel code segment (selector 0x08)
 *   and DS/ES/FS/GS/SS point to the kernel data segment (selector 0x10).
 * =============================================================================
 */

#ifndef DUGOS_GDT_H
#define DUGOS_GDT_H

#include <stdint.h>  /* uint8_t, uint16_t, uint32_t */

/* =============================================================================
 * gdt_init() -- build the 5-entry GDT and load it into the CPU
 *
 * Entries installed:
 *   0 -- Null descriptor       (required by the CPU spec; must always be zero)
 *   1 -- Kernel code segment   (selector 0x08, ring 0, executable, readable)
 *   2 -- Kernel data segment   (selector 0x10, ring 0, writable)
 *   3 -- User code segment     (selector 0x18, ring 3, executable, readable)
 *   4 -- User data segment     (selector 0x20, ring 3, writable)
 *
 * All segments use a flat memory model: base=0, limit=4 GiB. Memory
 * protection is handled by paging (not yet implemented).
 *
 * Internally calls gdt_flush() (in isr_stubs.s) to execute the lgdt
 * instruction and perform a far jump to reload the CS register.
 * =============================================================================
 */
void gdt_init(void);

#endif /* DUGOS_GDT_H */
