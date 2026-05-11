/* DugOS -- Interrupt Descriptor Table.
 *
 * Installs handlers for the 32 CPU exceptions (vectors 0-31).
 * IRQs (vectors 32-47) are wired up later in B.2 (intr_init).
 */

#include "idt.h"

#define IDT_ENTRIES 256

struct idt_entry {
    uint16_t handler_low;
    uint16_t selector;
    uint8_t  always0;
    uint8_t  flags;
    uint16_t handler_high;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

static struct idt_entry idt[IDT_ENTRIES];
static struct idt_ptr   idt_descriptor;

extern void idt_flush(uint32_t);   /* in isr.s */

/* ISR stubs declared in isr.s -- one per exception vector. */
extern void isr0 (void); extern void isr1 (void); extern void isr2 (void); extern void isr3 (void);
extern void isr4 (void); extern void isr5 (void); extern void isr6 (void); extern void isr7 (void);
extern void isr8 (void); extern void isr9 (void); extern void isr10(void); extern void isr11(void);
extern void isr12(void); extern void isr13(void); extern void isr14(void); extern void isr15(void);
extern void isr16(void); extern void isr17(void); extern void isr18(void); extern void isr19(void);
extern void isr20(void); extern void isr21(void); extern void isr22(void); extern void isr23(void);
extern void isr24(void); extern void isr25(void); extern void isr26(void); extern void isr27(void);
extern void isr28(void); extern void isr29(void); extern void isr30(void); extern void isr31(void);

static void idt_set(int n, uint32_t handler, uint16_t sel, uint8_t flags)
{
    idt[n].handler_low  = handler & 0xFFFF;
    idt[n].handler_high = (handler >> 16) & 0xFFFF;
    idt[n].selector     = sel;
    idt[n].always0      = 0;
    idt[n].flags        = flags;
}

void idt_init(void)
{
    idt_descriptor.limit = sizeof(idt) - 1;
    idt_descriptor.base  = (uint32_t) &idt;

    /* Zero everything; unused vectors stay null. */
    uint8_t *p = (uint8_t *) idt;
    for (uint32_t i = 0; i < sizeof(idt); i++) p[i] = 0;

    /* 0x08 = kernel code segment.  0x8E = present, ring 0, 32-bit interrupt gate. */
    idt_set(0,  (uint32_t) isr0,  0x08, 0x8E);
    idt_set(1,  (uint32_t) isr1,  0x08, 0x8E);
    idt_set(2,  (uint32_t) isr2,  0x08, 0x8E);
    idt_set(3,  (uint32_t) isr3,  0x08, 0x8E);
    idt_set(4,  (uint32_t) isr4,  0x08, 0x8E);
    idt_set(5,  (uint32_t) isr5,  0x08, 0x8E);
    idt_set(6,  (uint32_t) isr6,  0x08, 0x8E);
    idt_set(7,  (uint32_t) isr7,  0x08, 0x8E);
    idt_set(8,  (uint32_t) isr8,  0x08, 0x8E);
    idt_set(9,  (uint32_t) isr9,  0x08, 0x8E);
    idt_set(10, (uint32_t) isr10, 0x08, 0x8E);
    idt_set(11, (uint32_t) isr11, 0x08, 0x8E);
    idt_set(12, (uint32_t) isr12, 0x08, 0x8E);
    idt_set(13, (uint32_t) isr13, 0x08, 0x8E);
    idt_set(14, (uint32_t) isr14, 0x08, 0x8E);
    idt_set(15, (uint32_t) isr15, 0x08, 0x8E);
    idt_set(16, (uint32_t) isr16, 0x08, 0x8E);
    idt_set(17, (uint32_t) isr17, 0x08, 0x8E);
    idt_set(18, (uint32_t) isr18, 0x08, 0x8E);
    idt_set(19, (uint32_t) isr19, 0x08, 0x8E);
    idt_set(20, (uint32_t) isr20, 0x08, 0x8E);
    idt_set(21, (uint32_t) isr21, 0x08, 0x8E);
    idt_set(22, (uint32_t) isr22, 0x08, 0x8E);
    idt_set(23, (uint32_t) isr23, 0x08, 0x8E);
    idt_set(24, (uint32_t) isr24, 0x08, 0x8E);
    idt_set(25, (uint32_t) isr25, 0x08, 0x8E);
    idt_set(26, (uint32_t) isr26, 0x08, 0x8E);
    idt_set(27, (uint32_t) isr27, 0x08, 0x8E);
    idt_set(28, (uint32_t) isr28, 0x08, 0x8E);
    idt_set(29, (uint32_t) isr29, 0x08, 0x8E);
    idt_set(30, (uint32_t) isr30, 0x08, 0x8E);
    idt_set(31, (uint32_t) isr31, 0x08, 0x8E);

    idt_flush((uint32_t) &idt_descriptor);
}
