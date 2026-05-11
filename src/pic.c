/* =============================================================================
 * pic.c -- DugOS 8259A Programmable Interrupt Controller implementation
 *
 * PURPOSE:
 *   Implements initialization and control of the dual 8259A PIC chip set.
 *   The key operation is remapping hardware IRQs away from the CPU exception
 *   vectors so that keyboard and timer interrupts reach our IRQ handlers
 *   instead of crashing the OS.
 *
 * 8259A INITIALIZATION SEQUENCE:
 *   Both PICs (master and slave) are programmed with four Initialization
 *   Command Words (ICW1-ICW4) sent in sequence:
 *
 *   ICW1 (to command port):
 *     0x11 = start init sequence, cascade mode, ICW4 needed, edge-triggered
 *
 *   ICW2 (to data port):
 *     Vector base offset -- where IRQs appear in the IDT
 *     Master: 0x20 (32) -- so IRQ0 = vector 32, IRQ7 = vector 39
 *     Slave:  0x28 (40) -- so IRQ8 = vector 40, IRQ15 = vector 47
 *
 *   ICW3 (to data port):
 *     Master: 0x04 = bit 2 set -- IRQ2 line carries the slave cascade signal
 *     Slave:  0x02 -- slave identity; it is attached to master's IRQ2 line
 *
 *   ICW4 (to data port):
 *     0x01 = 8086/88 mode (not MCS-80 mode), normal EOI, non-buffered
 *
 * REFERENCES:
 *   MINIX 3 source: reference/minix/kernel/i8259.c (intr_init function)
 *   Intel 8259A datasheet, Section 3 (Initialization)
 *   OSDev Wiki -- 8259 PIC: https://wiki.osdev.org/8259_PIC
 * =============================================================================
 */

#include "pic.h"
#include "port.h"  /* outb, inb, io_wait */
#include "vga.h"   /* vga_writeln, vga_set_color for boot messages */

/* Master PIC I/O ports */
#define PIC1_CMD   0x20   /* master command register (write) / status (read) */
#define PIC1_DATA  0x21   /* master interrupt mask register (IMR)             */

/* Slave PIC I/O ports */
#define PIC2_CMD   0xA0   /* slave command register                           */
#define PIC2_DATA  0xA1   /* slave interrupt mask register                    */

/* ICW1: 0x11 = initialize + ICW4 needed + cascade mode + edge-triggered */
#define ICW1_INIT  0x11

/* ICW4: 0x01 = 8086/88 mode, normal (not AEOI) EOI, non-buffered */
#define ICW4_8086  0x01

/* EOI (End-Of-Interrupt) command word for OCW2 */
#define PIC_EOI    0x20

/* Vector base addresses after remapping */
#define PIC1_OFFSET 0x20   /* master: IRQ 0-7  --> vectors 32-39  */
#define PIC2_OFFSET 0x28   /* slave:  IRQ 8-15 --> vectors 40-47  */

/* BIOS default vector bases (used when restoring for shutdown) */
#define BIOS_PIC1_OFFSET 0x08   /* BIOS master base: vectors 8-15  */
#define BIOS_PIC2_OFFSET 0x70   /* BIOS slave base:  vectors 112-119 */

/* =============================================================================
 * pic_init() -- remap both PICs and mask all IRQ lines
 *
 * Mirrors MINIX's intr_init(1) from reference/minix/kernel/i8259.c.
 * Saves and restores the existing IRQ masks so that lines already enabled
 * by the BIOS remain enabled after the remap (the remap itself resets masks).
 * =============================================================================
 */
void pic_init(void)
{
    /* Save the current mask registers so we can restore them after remapping.
     * A remap resets the masks, so we must save them now. */
    uint8_t mask1 = inb(PIC1_DATA);
    uint8_t mask2 = inb(PIC2_DATA);

    /* ICW1: send initialization command to both PICs.
     * This starts the init sequence; the next three writes go to the data port. */
    outb(PIC1_CMD,  ICW1_INIT); io_wait();   /* begin init: master PIC */
    outb(PIC2_CMD,  ICW1_INIT); io_wait();   /* begin init: slave PIC  */

    /* ICW2: tell each PIC where its IRQs start in the IDT (vector offset). */
    outb(PIC1_DATA, PIC1_OFFSET); io_wait(); /* master: IRQ 0-7  at vectors 0x20+ */
    outb(PIC2_DATA, PIC2_OFFSET); io_wait(); /* slave:  IRQ 8-15 at vectors 0x28+ */

    /* ICW3: tell the PICs about the cascade (slave-to-master) connection.
     * Master: bit 2 = 0x04 = IRQ2 pin is the cascade input from the slave.
     * Slave:  binary 0x02 = this slave is attached to master IRQ2 (its ID). */
    outb(PIC1_DATA, 0x04); io_wait();
    outb(PIC2_DATA, 0x02); io_wait();

    /* ICW4: set 8086/88 mode (as opposed to MCS-80 mode). */
    outb(PIC1_DATA, ICW4_8086); io_wait();
    outb(PIC2_DATA, ICW4_8086); io_wait();

    /* Restore the saved masks. All lines start masked; callers unmask
     * specific IRQs as their drivers are initialized. */
    outb(PIC1_DATA, mask1);
    outb(PIC2_DATA, mask2);

    /* Print confirmation to the VGA screen as a boot-stage status line. */
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_writeln("  >> PIC remapped (IRQ 0-7 -> 0x20, IRQ 8-15 -> 0x28)");
}

/* =============================================================================
 * pic_restore() -- restore BIOS PIC defaults and mask everything
 *
 * Mirrors MINIX's intr_init(0) from reference/minix/kernel/i8259.c.
 * Called during shutdown so the machine returns to BIOS-compatible state
 * before the power-off command is issued.
 * =============================================================================
 */
void pic_restore(void)
{
    /* Reinitialize both PICs at their BIOS default vector offsets. */
    outb(PIC1_CMD,  ICW1_INIT); io_wait();
    outb(PIC2_CMD,  ICW1_INIT); io_wait();
    outb(PIC1_DATA, BIOS_PIC1_OFFSET); io_wait();
    outb(PIC2_DATA, BIOS_PIC2_OFFSET); io_wait();
    outb(PIC1_DATA, 0x04); io_wait();
    outb(PIC2_DATA, 0x02); io_wait();
    outb(PIC1_DATA, ICW4_8086); io_wait();
    outb(PIC2_DATA, ICW4_8086); io_wait();

    /* Mask all IRQ lines so no interrupts fire after shutdown. */
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
}

/* =============================================================================
 * pic_eoi() -- signal End-Of-Interrupt to the PIC(s)
 *
 * Must be called at the end of every hardware IRQ handler. The PIC will not
 * deliver further interrupts on the same line until it receives EOI.
 * For slave-PIC IRQs (8-15), both the slave and master must receive EOI.
 * =============================================================================
 */
void pic_eoi(uint8_t irq)
{
    if (irq >= 8) {
        /* Slave IRQ: send EOI to slave first, then to master (cascade). */
        outb(PIC2_CMD, PIC_EOI);
    }
    /* Always send EOI to master (either directly for IRQ 0-7, or for the
     * cascade line on IRQ2 when the slave IRQ was the real source). */
    outb(PIC1_CMD, PIC_EOI);
}

/* =============================================================================
 * pic_mask() -- disable one IRQ line (set the mask bit)
 *
 * Sets the corresponding bit in the PIC's Interrupt Mask Register (IMR).
 * A masked IRQ is still latched by the PIC hardware but never forwarded
 * to the CPU.
 * =============================================================================
 */
void pic_mask(uint8_t irq)
{
    uint16_t port;
    uint8_t  val;

    if (irq < 8) {
        port = PIC1_DATA;          /* IRQ 0-7:  master PIC data port */
    } else {
        port = PIC2_DATA;          /* IRQ 8-15: slave PIC data port  */
        irq -= 8;                  /* convert to slave-relative bit  */
    }
    val = inb(port) | (uint8_t)(1u << irq);  /* set the mask bit */
    outb(port, val);
}

/* =============================================================================
 * pic_unmask() -- enable one IRQ line (clear the mask bit)
 *
 * Clears the corresponding bit in the PIC's IMR so that the CPU receives
 * interrupts from that hardware line.
 * =============================================================================
 */
void pic_unmask(uint8_t irq)
{
    uint16_t port;
    uint8_t  val;

    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    val = inb(port) & (uint8_t)(~(1u << irq)); /* clear the mask bit */
    outb(port, val);
}
