/* =============================================================================
 * idt.c -- DugOS Interrupt Descriptor Table implementation
 *
 * PURPOSE:
 *   Builds the 256-entry Interrupt Descriptor Table and installs handlers
 *   for all 32 CPU exception vectors (0-31). Hardware IRQ vectors (32-47)
 *   will be added here in Phase B.2 when the 8259A PIC is remapped.
 *
 * IDT ENTRY FORMAT (8 bytes, defined by Intel CPU spec):
 *   Bits 15-0  (handler_low)  -- lower 16 bits of handler function address
 *   Bits 31-16 (selector)     -- GDT segment selector to use when jumping
 *   Bits 39-32 (always0)      -- must be zero (reserved by CPU)
 *   Bits 47-40 (flags)        -- gate type and privilege:
 *                                  bit 7:   Present (P) = 1 (entry is valid)
 *                                  bits 6-5: DPL = 00 (ring 0, kernel only)
 *                                  bit 4:   Storage segment = 0
 *                                  bits 3-0: Gate type:
 *                                    0x8E = 32-bit interrupt gate
 *                                           (also disables interrupts on entry)
 *                                    0x8F = 32-bit trap gate
 *                                           (does NOT disable interrupts)
 *   Bits 63-48 (handler_high) -- upper 16 bits of handler function address
 *
 * INTERRUPT GATE vs TRAP GATE:
 *   We use interrupt gates (0x8E) for all exception handlers. An interrupt
 *   gate automatically clears the IF (interrupt-enable) flag on entry, so
 *   no nested interrupts can occur while we are handling an exception.
 *
 * REFERENCES:
 *   Intel SDM Vol.3, Chapter 6 (Interrupt and Exception Handling)
 *   OSDev Wiki -- IDT: https://wiki.osdev.org/IDT
 *   MINIX reference: reference/minix/kernel/protect.c
 * =============================================================================
 */

#include "idt.h"

/* Total number of IDT entries. The x86 architecture defines 256 vectors. */
#define IDT_ENTRIES 256

/* =============================================================================
 * struct idt_entry -- one 8-byte gate descriptor in the IDT
 *
 * __attribute__((packed)) prevents compiler padding so the layout matches
 * exactly what the CPU expects when it reads the IDT.
 * =============================================================================
 */
struct idt_entry {
    uint16_t handler_low;   /* bits 15-0  of the ISR handler address    */
    uint16_t selector;      /* GDT segment selector (0x08 = kernel code) */
    uint8_t  always0;       /* reserved -- must always be set to 0       */
    uint8_t  flags;         /* gate type + DPL + present bit (see above) */
    uint16_t handler_high;  /* bits 31-16 of the ISR handler address     */
} __attribute__((packed));

/* =============================================================================
 * struct idt_ptr -- the 6-byte structure passed to the lidt instruction
 *
 * Same layout as gdt_ptr: 2-byte size limit followed by 4-byte linear
 * address of the IDT array.
 * =============================================================================
 */
struct idt_ptr {
    uint16_t limit;   /* size of IDT in bytes minus 1 */
    uint32_t base;    /* linear address of idt[] array */
} __attribute__((packed));

/* The IDT array and its descriptor. Static: not visible outside this file. */
static struct idt_entry idt[IDT_ENTRIES];
static struct idt_ptr   idt_descriptor;

/* idt_flush() is in isr_stubs.s -- executes lidt with the given address. */
extern void idt_flush(uint32_t);

/* ISR stub functions, one per CPU exception vector (0-31).
 * Each is a short NASM routine in isr_stubs.s that pushes the vector number
 * (and a dummy error code for vectors that don't push one) then jumps to
 * isr_common, which saves CPU state and calls isr_common_handler() in isr.c. */
extern void isr0 (void); extern void isr1 (void); extern void isr2 (void);
extern void isr3 (void); extern void isr4 (void); extern void isr5 (void);
extern void isr6 (void); extern void isr7 (void); extern void isr8 (void);
extern void isr9 (void); extern void isr10(void); extern void isr11(void);
extern void isr12(void); extern void isr13(void); extern void isr14(void);
extern void isr15(void); extern void isr16(void); extern void isr17(void);
extern void isr18(void); extern void isr19(void); extern void isr20(void);
extern void isr21(void); extern void isr22(void); extern void isr23(void);
extern void isr24(void); extern void isr25(void); extern void isr26(void);
extern void isr27(void); extern void isr28(void); extern void isr29(void);
extern void isr30(void); extern void isr31(void);

/* =============================================================================
 * idt_set() -- write one gate descriptor into the IDT array
 *
 * Parameters:
 *   n       -- IDT vector index (0-255)
 *   handler -- 32-bit address of the ISR stub function
 *   sel     -- GDT selector to load into CS when the gate fires
 *               (always 0x08 = kernel code segment for our exception gates)
 *   flags   -- gate type and privilege byte (0x8E for kernel interrupt gate)
 *
 * Splits the handler address into low and high 16-bit halves as required
 * by the IDT entry format, then stores all fields in idt[n].
 * =============================================================================
 */
static void idt_set(int n, uint32_t handler, uint16_t sel, uint8_t flags)
{
    idt[n].handler_low  = handler & 0xFFFF;          /* lower 16 bits of ISR address */
    idt[n].handler_high = (handler >> 16) & 0xFFFF;  /* upper 16 bits of ISR address */
    idt[n].selector     = sel;      /* kernel code segment selector             */
    idt[n].always0      = 0;        /* reserved field -- must be zero           */
    idt[n].flags        = flags;    /* 0x8E = present, ring 0, interrupt gate   */
}

/* =============================================================================
 * idt_init() -- initialise the full 256-entry IDT and load it
 *
 * Steps:
 *   1. Zero all 256 IDT entries so unused vectors are null/invalid.
 *   2. Install ISR stubs for CPU exception vectors 0-31.
 *   3. Load the IDT descriptor via idt_flush() (lidt instruction).
 *
 * Hardware IRQ vectors (32-47) are added in Phase B.2 when the 8259A
 * Programmable Interrupt Controller is remapped.
 *
 * Selector 0x08 = kernel code segment (GDT entry 1, set up in gdt_init).
 * Flags    0x8E = 1000 1110:
 *   bit 7 (P)    = 1 (present)
 *   bits 6-5     = 00 (DPL ring 0 -- only kernel code can trigger these)
 *   bit 4        = 0 (not a storage segment descriptor)
 *   bits 3-0     = 1110 (32-bit interrupt gate)
 * =============================================================================
 */
void idt_init(void)
{
    /* Set up the IDT descriptor for the lidt instruction. */
    idt_descriptor.limit = sizeof(idt) - 1;       /* total size minus 1        */
    idt_descriptor.base  = (uint32_t) &idt;        /* address of idt[] array    */

    /* Zero all 256 entries. Unused vectors will cause a double fault if they
     * fire, which IS handled (vector 8) and gives us a diagnostic message. */
    uint8_t *p = (uint8_t *) idt;
    for (uint32_t i = 0; i < sizeof(idt); i++) p[i] = 0;

    /* Install the 32 CPU exception handlers (vectors 0-31).
     * Each isrN function is an assembly stub in isr_stubs.s. */
    idt_set(0,  (uint32_t) isr0,  0x08, 0x8E);  /* Divide by zero              */
    idt_set(1,  (uint32_t) isr1,  0x08, 0x8E);  /* Debug exception             */
    idt_set(2,  (uint32_t) isr2,  0x08, 0x8E);  /* Non-maskable interrupt      */
    idt_set(3,  (uint32_t) isr3,  0x08, 0x8E);  /* Breakpoint (INT 3)          */
    idt_set(4,  (uint32_t) isr4,  0x08, 0x8E);  /* Overflow (INTO)             */
    idt_set(5,  (uint32_t) isr5,  0x08, 0x8E);  /* Bound range exceeded        */
    idt_set(6,  (uint32_t) isr6,  0x08, 0x8E);  /* Invalid opcode              */
    idt_set(7,  (uint32_t) isr7,  0x08, 0x8E);  /* Device not available        */
    idt_set(8,  (uint32_t) isr8,  0x08, 0x8E);  /* Double fault (has err code) */
    idt_set(9,  (uint32_t) isr9,  0x08, 0x8E);  /* Coprocessor segment overrun */
    idt_set(10, (uint32_t) isr10, 0x08, 0x8E);  /* Invalid TSS                 */
    idt_set(11, (uint32_t) isr11, 0x08, 0x8E);  /* Segment not present         */
    idt_set(12, (uint32_t) isr12, 0x08, 0x8E);  /* Stack-segment fault         */
    idt_set(13, (uint32_t) isr13, 0x08, 0x8E);  /* General protection fault    */
    idt_set(14, (uint32_t) isr14, 0x08, 0x8E);  /* Page fault                  */
    idt_set(15, (uint32_t) isr15, 0x08, 0x8E);  /* Reserved                    */
    idt_set(16, (uint32_t) isr16, 0x08, 0x8E);  /* x87 FPU exception           */
    idt_set(17, (uint32_t) isr17, 0x08, 0x8E);  /* Alignment check             */
    idt_set(18, (uint32_t) isr18, 0x08, 0x8E);  /* Machine check               */
    idt_set(19, (uint32_t) isr19, 0x08, 0x8E);  /* SIMD floating-point         */
    idt_set(20, (uint32_t) isr20, 0x08, 0x8E);  /* Virtualization exception    */
    idt_set(21, (uint32_t) isr21, 0x08, 0x8E);  /* Reserved                    */
    idt_set(22, (uint32_t) isr22, 0x08, 0x8E);  /* Reserved                    */
    idt_set(23, (uint32_t) isr23, 0x08, 0x8E);  /* Reserved                    */
    idt_set(24, (uint32_t) isr24, 0x08, 0x8E);  /* Reserved                    */
    idt_set(25, (uint32_t) isr25, 0x08, 0x8E);  /* Reserved                    */
    idt_set(26, (uint32_t) isr26, 0x08, 0x8E);  /* Reserved                    */
    idt_set(27, (uint32_t) isr27, 0x08, 0x8E);  /* Reserved                    */
    idt_set(28, (uint32_t) isr28, 0x08, 0x8E);  /* Reserved                    */
    idt_set(29, (uint32_t) isr29, 0x08, 0x8E);  /* Reserved                    */
    idt_set(30, (uint32_t) isr30, 0x08, 0x8E);  /* Security exception          */
    idt_set(31, (uint32_t) isr31, 0x08, 0x8E);  /* Reserved                    */

    /* Load the IDT: call the assembly helper which executes the lidt
     * instruction, making the CPU aware of our new interrupt table. */
    idt_flush((uint32_t) &idt_descriptor);
}
