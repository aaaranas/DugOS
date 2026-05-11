/* =============================================================================
 * port.h -- DugOS x86 port I/O inline helpers
 *
 * PURPOSE:
 *   Provides outb() and inb() as static inline functions so any file can
 *   access x86 hardware I/O ports without duplicating inline asm everywhere.
 *   These are the only way to communicate with the PIC, PS/2 controller,
 *   CMOS clock, and other ISA-bus devices in a freestanding kernel.
 *
 * HOW x86 PORT I/O WORKS:
 *   The x86 has a separate 16-bit I/O address space (65536 ports, 0x0000-0xFFFF)
 *   accessed with the IN and OUT instructions. These instructions are privileged
 *   (ring 0 only), which is why they only appear in kernel code.
 *
 *   Common ports used by DugOS:
 *     0x20 / 0x21  -- Master 8259A PIC command / data
 *     0xA0 / 0xA1  -- Slave  8259A PIC command / data
 *     0x60         -- PS/2 keyboard data register
 *     0x64         -- PS/2 keyboard status/command register
 *     0x80         -- POST diagnostic port (used as ~1 us I/O delay)
 *     0x604        -- QEMU power-off port (ACPI PM1a control)
 *
 * REFERENCES:
 *   Intel SDM Vol.1, Section 13.4 (I/O Port Addressing)
 *   OSDev Wiki -- I/O Ports: https://wiki.osdev.org/I/O_Ports
 * =============================================================================
 */

#ifndef DUGOS_PORT_H
#define DUGOS_PORT_H

#include <stdint.h>

/* =============================================================================
 * outb() -- write one byte to a hardware I/O port
 *
 * Parameters:
 *   port -- 16-bit I/O port address
 *   val  -- byte value to send
 *
 * Uses AT&T asm: "outb %0, %1" means OUT port(%1), val(%0)
 * Constraint "a"  = value goes into AL (low byte of EAX)
 * Constraint "Nd" = port can be an immediate byte (N) or DX register (d)
 * =============================================================================
 */
static inline void outb(uint16_t port, uint8_t val)
{
    __asm__ __volatile__ ("outb %0, %1" : : "a"(val), "Nd"(port));
}

/* =============================================================================
 * inb() -- read one byte from a hardware I/O port
 *
 * Parameters:
 *   port -- 16-bit I/O port address
 *
 * Returns:
 *   The byte value read from the port.
 *
 * Constraint "=a"  = result is read from AL into val
 * Constraint "Nd"  = port can be an immediate byte or DX register
 * =============================================================================
 */
static inline uint8_t inb(uint16_t port)
{
    uint8_t val;
    __asm__ __volatile__ ("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

/* =============================================================================
 * io_wait() -- insert a ~1 microsecond I/O delay
 *
 * Writing to port 0x80 (the POST diagnostic port) is the standard way to
 * introduce a short delay when communicating with old ISA devices like the
 * 8259A PIC that need time between commands. The write is harmless.
 * =============================================================================
 */
static inline void io_wait(void)
{
    outb(0x80, 0);
}

#endif /* DUGOS_PORT_H */
