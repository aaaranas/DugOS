/* =============================================================================
 * gdt.c -- DugOS Global Descriptor Table implementation
 *
 * PURPOSE:
 *   Builds and loads a 5-entry Global Descriptor Table (GDT) that gives the
 *   CPU its memory segment configuration for 32-bit protected mode.
 *
 * WHAT IS THE GDT?
 *   In x86 32-bit protected mode, every memory access goes through a segment
 *   selector. The CPU looks up the selector in the GDT to find the segment's
 *   base address, size limit, privilege level (ring 0-3), and access flags.
 *   We use a "flat" model: every segment covers the full 4 GiB address space
 *   (base=0, limit=0xFFFFFFFF), so segmentation is effectively transparent.
 *
 * ACCESS BYTE FORMAT (8 bits):
 *   Bit 7   -- Present (P):     1 = descriptor is valid
 *   Bits 6-5 -- DPL:            privilege level (0=kernel, 3=user)
 *   Bit 4   -- Descriptor type: 1 = code/data, 0 = system
 *   Bit 3   -- Executable (E):  1 = code segment, 0 = data segment
 *   Bit 2   -- Direction/Conform
 *   Bit 1   -- Readable/Writable
 *   Bit 0   -- Accessed:        CPU sets this when segment is used
 *
 * FLAGS NIBBLE (high 4 bits of the flags byte in struct gdt_entry):
 *   Bit 3 (G):  Granularity -- 1 = limit in 4 KiB pages, 0 = limit in bytes
 *   Bit 2 (DB): 1 = 32-bit protected mode segment
 *   Bit 1:      Reserved (must be 0)
 *   Bit 0:      Limit bits 19-16 (low nibble of flags byte)
 *
 * REFERENCES:
 *   Intel SDM Vol.3, Chapter 3 (Protected-Mode Memory Management)
 *   OSDev Wiki -- GDT Tutorial: https://wiki.osdev.org/GDT_Tutorial
 *   MINIX reference: reference/minix/kernel/protect.c
 * =============================================================================
 */

#include "gdt.h"

/* Number of entries in our GDT: null + kernel code/data + user code/data. */
#define GDT_ENTRIES 5

/* =============================================================================
 * struct gdt_entry -- one 8-byte descriptor in the GDT
 *
 * The layout is defined by the Intel CPU spec and CANNOT be changed.
 * __attribute__((packed)) prevents the compiler from adding padding bytes
 * between fields, which would break the CPU's interpretation of the struct.
 * =============================================================================
 */
struct gdt_entry {
    uint16_t limit_low;   /* bits 15-0  of the segment limit                  */
    uint16_t base_low;    /* bits 15-0  of the segment base address            */
    uint8_t  base_mid;    /* bits 23-16 of the segment base address            */
    uint8_t  access;      /* access byte: P | DPL | S | E | DC | RW | A       */
    uint8_t  flags;       /* high nibble = flags (G|DB|0|0), low = limit 19-16 */
    uint8_t  base_high;   /* bits 31-24 of the segment base address            */
} __attribute__((packed));

/* =============================================================================
 * struct gdt_ptr -- the 6-byte structure passed to the lgdt instruction
 *
 * lgdt expects a pointer to this exact layout:
 *   - 2-byte limit: size of the GDT in bytes minus 1
 *   - 4-byte base:  linear address of the GDT array in memory
 * =============================================================================
 */
struct gdt_ptr {
    uint16_t limit;   /* GDT size in bytes minus 1 */
    uint32_t base;    /* physical address of gdt[]  */
} __attribute__((packed));

/* The actual GDT array and its descriptor, stored in the BSS segment.
 * Static so they are not visible outside this translation unit. */
static struct gdt_entry gdt[GDT_ENTRIES];
static struct gdt_ptr   gdt_descriptor;

/* gdt_flush() is implemented in isr_stubs.s. It calls lgdt with the address
 * of gdt_descriptor, reloads all segment registers to the kernel data
 * selector (0x10), and executes a far jump to reload CS to 0x08. */
extern void gdt_flush(uint32_t);

/* =============================================================================
 * gdt_set() -- fill one entry in the GDT array
 *
 * Parameters:
 *   idx    -- index into gdt[] (0-4)
 *   base   -- segment base address (32-bit linear address)
 *   limit  -- segment size limit (20 bits; interpreted as pages if G=1)
 *   access -- access byte (P | DPL | S | E | DC | RW | A)
 *   flags  -- high nibble only (G | DB | 0 | 0); low nibble is overwritten
 *             with the upper 4 bits of 'limit'
 *
 * The base and limit fields are split across non-contiguous bytes in the
 * struct to match the original 80286 descriptor format (a historical quirk
 * retained for backwards compatibility).
 * =============================================================================
 */
static void gdt_set(int idx, uint32_t base, uint32_t limit,
                    uint8_t access, uint8_t flags)
{
    /* Split the 32-bit base into three non-contiguous fields. */
    gdt[idx].base_low  = base & 0xFFFF;          /* bits 15-0  */
    gdt[idx].base_mid  = (base >> 16) & 0xFF;    /* bits 23-16 */
    gdt[idx].base_high = (base >> 24) & 0xFF;    /* bits 31-24 */

    /* Split the 20-bit limit: low 16 bits in limit_low, top 4 bits in flags. */
    gdt[idx].limit_low = limit & 0xFFFF;
    gdt[idx].flags     = ((limit >> 16) & 0x0F)  /* limit bits 19-16 in low nibble */
                       | (flags & 0xF0);          /* G|DB flags in high nibble      */

    gdt[idx].access = access;  /* store the access byte directly */
}

/* =============================================================================
 * gdt_init() -- build all 5 GDT entries and load the GDT into the CPU
 *
 * Access byte values used below:
 *   0x9A = 1001 1010 : P=1, DPL=00 (ring 0), S=1, E=1 (code), DC=0, R=1, A=0
 *   0x92 = 1001 0010 : P=1, DPL=00 (ring 0), S=1, E=0 (data), DC=0, W=1, A=0
 *   0xFA = 1111 1010 : P=1, DPL=11 (ring 3), S=1, E=1 (code), DC=0, R=1, A=0
 *   0xF2 = 1111 0010 : P=1, DPL=11 (ring 3), S=1, E=0 (data), DC=0, W=1, A=0
 *
 * Flags byte (high nibble) 0xCF = 1100 in high nibble:
 *   G=1  (granularity: limit in 4 KiB pages, so limit=0xFFFFF covers 4 GiB)
 *   DB=1 (32-bit segment)
 * =============================================================================
 */
void gdt_init(void)
{
    /* Set up the GDT descriptor (size and address) for the lgdt instruction. */
    gdt_descriptor.limit = sizeof(gdt) - 1;       /* size in bytes minus 1    */
    gdt_descriptor.base  = (uint32_t) &gdt;        /* address of gdt[] array   */

    /* Entry 0: Null descriptor -- the CPU spec requires the first entry to be
     * all zeros. Attempting to use selector 0x00 causes a general protection
     * fault, which helps catch uninitialized segment register bugs. */
    gdt_set(0, 0, 0, 0, 0);

    /* Entry 1: Kernel code segment (selector = 0x08 = index 1 << 3)
     * Ring 0, executable, readable, flat 4 GiB, 32-bit, 4 KiB granularity. */
    gdt_set(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);

    /* Entry 2: Kernel data segment (selector = 0x10 = index 2 << 3)
     * Ring 0, writable, flat 4 GiB, 32-bit, 4 KiB granularity. */
    gdt_set(2, 0, 0xFFFFFFFF, 0x92, 0xCF);

    /* Entry 3: User code segment (selector = 0x18 = index 3 << 3, RPL=3)
     * Ring 3 (user mode), executable, readable. Not yet used -- reserved for
     * future user-mode process support. */
    gdt_set(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);

    /* Entry 4: User data segment (selector = 0x20 = index 4 << 3, RPL=3)
     * Ring 3 (user mode), writable. Reserved for future user-mode support. */
    gdt_set(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);

    /* Load the GDT: call the assembly helper which executes lgdt, reloads
     * all segment registers, and far-jumps to flush the CS pipeline. */
    gdt_flush((uint32_t) &gdt_descriptor);
}
