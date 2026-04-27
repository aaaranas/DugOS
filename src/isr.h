#ifndef DUGOS_ISR_H
#define DUGOS_ISR_H

#include <stdint.h>

/* Layout matches what the ISR stubs in isr.s push onto the stack
 * before calling isr_common_handler. Lowest address (smallest field
 * offset) is what ESP points to at handler entry. */
struct regs {
    uint32_t gs, fs, es, ds;                          /* pushed by stub  */
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;  /* pusha           */
    uint32_t vector, err_code;                        /* pushed by stub  */
    uint32_t eip, cs, eflags, useresp, ss;            /* pushed by CPU   */
};

void isr_common_handler(struct regs *r);

#endif
