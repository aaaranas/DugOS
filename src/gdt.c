/* DugOS -- Global Descriptor Table.
 *
 * Sets up flat-memory segments for kernel and user mode in 32-bit
 * protected mode. Replaces the temporary GDT GRUB installed.
 */

#include "gdt.h"

#define GDT_ENTRIES 5

struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  flags;     /* high nibble = flags, low nibble = limit_high */
    uint8_t  base_high;
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

static struct gdt_entry gdt[GDT_ENTRIES];
static struct gdt_ptr   gdt_descriptor;

extern void gdt_flush(uint32_t);   /* implemented in isr.s */

static void gdt_set(int idx, uint32_t base, uint32_t limit,
                    uint8_t access, uint8_t flags)
{
    gdt[idx].base_low  = base & 0xFFFF;
    gdt[idx].base_mid  = (base >> 16) & 0xFF;
    gdt[idx].base_high = (base >> 24) & 0xFF;

    gdt[idx].limit_low = limit & 0xFFFF;
    gdt[idx].flags     = ((limit >> 16) & 0x0F) | (flags & 0xF0);
    gdt[idx].access    = access;
}

void gdt_init(void)
{
    gdt_descriptor.limit = sizeof(gdt) - 1;
    gdt_descriptor.base  = (uint32_t) &gdt;

    /* 0: null descriptor */
    gdt_set(0, 0, 0, 0, 0);
    /* 1: kernel code -- access=0x9A, flags=0xCF (4 KiB granularity, 32-bit) */
    gdt_set(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);
    /* 2: kernel data -- access=0x92 */
    gdt_set(2, 0, 0xFFFFFFFF, 0x92, 0xCF);
    /* 3: user code   -- access=0xFA (DPL=3) */
    gdt_set(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);
    /* 4: user data   -- access=0xF2 */
    gdt_set(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);

    gdt_flush((uint32_t) &gdt_descriptor);
}
