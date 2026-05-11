; =============================================================================
; isr_stubs.s -- DugOS ISR assembly stubs, GDT flush, and IDT flush helpers
;
; PURPOSE:
;   Provides three things:
;     1. gdt_flush()    -- loads a new GDT and reloads all segment registers
;     2. idt_flush()    -- loads a new IDT via the lidt instruction
;     3. isrN stubs     -- one per CPU exception vector (0-31); each stub
;                          pushes a vector number and error code onto the
;                          stack then jumps to the common handler below
;
; WHY ASSEMBLY IS NEEDED HERE:
;   The lgdt and lidt instructions can only be executed in assembly (no C
;   intrinsic exists). The far jump after lgdt (to reload CS) also requires
;   assembly. The ISR stubs must save/restore ALL CPU registers in exactly
;   the right order to match struct regs in isr.h -- C compilers do not
;   guarantee the register save order needed here.
;
; STRUCT REGS COUPLING:
;   The push order in isr_common MUST match the field order in struct regs
;   (src/isr.h). If either is changed, both must be updated together.
;
;   Stack layout at the point where isr_common_handler() is called
;   (lowest address / ESP+0 is listed first):
;     ESP+ 0 : gs          pushed by isr_common
;     ESP+ 4 : fs
;     ESP+ 8 : es
;     ESP+12 : ds
;     ESP+16 : edi         pushed by pusha (edi is at lowest addr after pusha)
;     ESP+20 : esi
;     ESP+24 : ebp
;     ESP+28 : esp_orig
;     ESP+32 : ebx
;     ESP+36 : edx
;     ESP+40 : ecx
;     ESP+44 : eax
;     ESP+48 : vector      pushed by ISR stub
;     ESP+52 : err_code    pushed by ISR stub (or CPU for vectors 8,10-14,17,30)
;     ESP+56 : eip         pushed by CPU (return address)
;     ESP+60 : cs
;     ESP+64 : eflags
;     (ESP+68 : useresp -- only present on privilege change)
;     (ESP+72 : ss      -- only present on privilege change)
;
; REFERENCES:
;   Intel SDM Vol.3, Section 6.12 (Exception and Interrupt Handling in 64-bit Mode)
;   OSDev Wiki -- Interrupt Service Routines: https://wiki.osdev.org/ISR
; =============================================================================

bits 32     ; all code in this file is 32-bit protected mode

; =============================================================================
; gdt_flush(uint32_t gdt_descriptor_addr)
;
; PURPOSE:
;   Load the new GDT (built in gdt.c) into the CPU and reload all segment
;   registers to point to the kernel data and code segments we defined.
;
; PARAMETER (via cdecl calling convention):
;   [esp+4] = physical address of the gdt_ptr struct (limit + base)
;
; HOW IT WORKS:
;   1. lgdt [eax]  -- load the GDT descriptor; CPU now knows where our GDT is
;   2. Reload DS, ES, FS, GS, SS to 0x10 (kernel data segment, GDT entry 2)
;   3. Far jump to 0x08:.flush -- this is the only way to reload CS (the code
;      segment register). A far jump takes a segment:offset operand; using
;      selector 0x08 (kernel code, GDT entry 1) as the segment reloads CS
;      from our GDT. The ':flush' label is the offset -- where we resume.
; =============================================================================
global gdt_flush        ; make visible to C linker (called from gdt.c)
gdt_flush:
    mov eax, [esp + 4]  ; load argument: address of gdt_ptr struct
    lgdt [eax]          ; tell the CPU about our new GDT (lgdt = load GDT register)

    ; Reload all data segment registers to our kernel data segment (0x10).
    ; After lgdt the CPU still uses the old segment values until we reload them.
    mov ax, 0x10        ; 0x10 = kernel data segment selector (GDT entry 2)
    mov ds, ax          ; DS: data segment
    mov es, ax          ; ES: extra segment (string operations)
    mov fs, ax          ; FS: additional segment
    mov gs, ax          ; GS: additional segment
    mov ss, ax          ; SS: stack segment

    ; A far jump is required to reload CS (code segment register).
    ; Format: jmp <selector>:<offset>
    ;   0x08 = kernel code segment selector (GDT entry 1)
    ;   .flush = label immediately after this jump (execution resumes there)
    jmp 0x08:.flush
.flush:
    ret                 ; return to gdt_init() in gdt.c

; =============================================================================
; idt_flush(uint32_t idt_descriptor_addr)
;
; PURPOSE:
;   Load the Interrupt Descriptor Table built in idt.c into the CPU.
;
; PARAMETER (via cdecl calling convention):
;   [esp+4] = physical address of the idt_ptr struct (limit + base)
;
; HOW IT WORKS:
;   lidt loads the IDT register (IDTR) with the 6-byte descriptor at [eax].
;   After this returns, the CPU uses our IDT to dispatch all interrupts and
;   exceptions. Interrupts remain disabled (cli was set in boot.s) until
;   'sti' is explicitly called (will happen in Phase B.2 intr_init).
; =============================================================================
global idt_flush        ; make visible to C linker (called from idt.c)
idt_flush:
    mov eax, [esp + 4]  ; load argument: address of idt_ptr struct
    lidt [eax]          ; tell the CPU about our IDT (lidt = load IDT register)
    ret                 ; return to idt_init() in idt.c

; =============================================================================
; ISR STUB MACROS
;
; The CPU can only jump to a flat 32-bit address when an interrupt fires --
; it cannot pass arguments to C directly. We therefore create a small
; assembly stub for each of the 32 exception vectors. Each stub:
;   1. Pushes the vector number so isr_common_handler() knows which exception
;      fired (the CPU does NOT push this automatically).
;   2. For vectors that do NOT push an error code, pushes a dummy 0 first
;      so that struct regs always has the same layout regardless of vector.
;   3. Jumps to isr_common (below) which handles the rest.
;
; ISR_NOERR <vector> -- use for vectors where the CPU pushes no error code.
;   Pushes: dummy error code (0), vector number, then jumps to isr_common.
;
; ISR_ERR <vector>   -- use for vectors where the CPU pushes a real error code.
;   The CPU already pushed the error code before jumping here, so we only
;   need to push the vector number.
;
; Vectors that push a real error code: 8, 10, 11, 12, 13, 14, 17, 30.
; All others use ISR_NOERR.
; =============================================================================
%macro ISR_NOERR 1
global isr%1            ; make isrN visible so idt.c can take its address
isr%1:
    cli                 ; disable interrupts (interrupt gate does this, but be explicit)
    push dword 0        ; push dummy error code (CPU did not push one for this vector)
    push dword %1       ; push the vector number (e.g. push 6 for invalid opcode)
    jmp isr_common      ; jump to common handler below
%endmacro

%macro ISR_ERR 1
global isr%1            ; make isrN visible so idt.c can take its address
isr%1:
    cli                 ; disable interrupts
    ; NOTE: the CPU already pushed a real error code onto the stack before
    ; jumping here -- we do NOT push a dummy 0 for these vectors.
    push dword %1       ; push the vector number
    jmp isr_common      ; jump to common handler below
%endmacro

; Instantiate the 32 ISR stubs using the macros above.
; Vectors 8, 10-14, 17, 30 get the ISR_ERR variant (CPU pushes error code).
; All others get ISR_NOERR (we push a dummy 0 to keep struct regs uniform).
ISR_NOERR 0   ; Divide by zero
ISR_NOERR 1   ; Debug
ISR_NOERR 2   ; Non-maskable interrupt
ISR_NOERR 3   ; Breakpoint (INT 3)
ISR_NOERR 4   ; Overflow
ISR_NOERR 5   ; Bound range exceeded
ISR_NOERR 6   ; Invalid opcode
ISR_NOERR 7   ; Device not available (no math coprocessor)
ISR_ERR   8   ; Double fault             -- CPU pushes error code (always 0)
ISR_NOERR 9   ; Coprocessor segment overrun (legacy, no error code)
ISR_ERR   10  ; Invalid TSS              -- CPU pushes selector error code
ISR_ERR   11  ; Segment not present      -- CPU pushes selector error code
ISR_ERR   12  ; Stack-segment fault      -- CPU pushes error code
ISR_ERR   13  ; General protection fault -- CPU pushes error code
ISR_ERR   14  ; Page fault               -- CPU pushes fault address info
ISR_NOERR 15  ; Reserved by Intel
ISR_NOERR 16  ; x87 FPU exception
ISR_ERR   17  ; Alignment check          -- CPU pushes error code (always 0)
ISR_NOERR 18  ; Machine check
ISR_NOERR 19  ; SIMD FP exception
ISR_NOERR 20  ; Virtualization exception
ISR_NOERR 21  ; Reserved
ISR_NOERR 22  ; Reserved
ISR_NOERR 23  ; Reserved
ISR_NOERR 24  ; Reserved
ISR_NOERR 25  ; Reserved
ISR_NOERR 26  ; Reserved
ISR_NOERR 27  ; Reserved
ISR_NOERR 28  ; Reserved
ISR_NOERR 29  ; Reserved
ISR_ERR   30  ; Security exception       -- CPU pushes error code
ISR_NOERR 31  ; Reserved

; =============================================================================
; isr_common -- shared assembly handler entered from every ISR stub above
;
; At entry, the stack looks like (high to low address):
;   [CPU-pushed: SS, useresp, EFLAGS, CS, EIP]  (SS/useresp only on priv change)
;   [err_code]    pushed by stub (real or dummy 0)
;   [vector]      pushed by stub
;
; This routine saves all remaining registers, ensures kernel data segments
; are loaded, then calls the C handler isr_common_handler(struct regs *r).
; On return from the C handler, it restores everything and executes iret.
; (For Phase B.1 the C handler always halts so iret is never reached.)
;
; REGISTER SAVE SEQUENCE:
;   pusha saves (in order, highest addr first): EAX, ECX, EDX, EBX,
;     ESP_orig, EBP, ESI, EDI. After pusha, ESP points to EDI.
;   Then DS, ES, FS, GS are pushed individually.
;   Finally, ESP itself is pushed as the argument to isr_common_handler --
;   it points to the bottom of the struct regs frame (the 'gs' field).
; =============================================================================
extern isr_common_handler   ; C function declared in isr.h, defined in isr.c

isr_common:
    ; Save all general-purpose registers onto the stack.
    ; 'pusha' pushes EAX, ECX, EDX, EBX, ESP(original), EBP, ESI, EDI.
    pusha

    ; Save segment registers. We push them individually (no 'pusha' equivalent).
    push ds
    push es
    push fs
    push gs

    ; Switch all data segment registers to the kernel data segment (0x10).
    ; The ISR may have fired while user-mode code was running (future phase),
    ; so we cannot assume DS etc. already point to the kernel segment.
    mov ax, 0x10    ; kernel data segment selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Pass a pointer to the saved register frame as the argument to the C
    ; handler. At this point ESP points to the bottom of the frame (field 'gs'
    ; in struct regs). The C handler receives it as: void handler(struct regs *r).
    push esp
    call isr_common_handler ; call the C handler (see isr.c)
    add esp, 4              ; pop the 'esp' argument we pushed above

    ; Restore segment registers in reverse push order.
    pop gs
    pop fs
    pop es
    pop ds

    ; Restore general-purpose registers (reverse of pusha).
    popa

    ; Remove the vector number and error code that the stub pushed.
    ; They are no longer needed; the iret instruction uses EIP/CS/EFLAGS
    ; from above them on the stack.
    add esp, 8

    ; Return from the interrupt/exception. iret pops EIP, CS, and EFLAGS
    ; and resumes execution at the interrupted instruction (or the next one,
    ; depending on the exception type). Interrupts are re-enabled if EFLAGS.IF
    ; was set before the exception.
    iret

; =============================================================================
; IRQ STUB MACRO AND STUBS (vectors 32-47) -- hardware interrupt handlers
;
; Hardware IRQs are different from CPU exceptions:
;   - They are triggered by peripherals (timer, keyboard, disk, etc.)
;   - They must NOT halt the CPU -- they must dispatch a handler and return.
;   - After servicing, the handler MUST send EOI to the PIC (done in C).
;
; IRQ_STUB <irq_nr>:
;   Saves all general-purpose and segment registers (in the same order as
;   isr_common so the compiler knows the stack is consistent), loads the
;   kernel data segment, calls irq_dispatch(irq_nr) in isr.c which looks
;   up the registered handler, calls it, and sends EOI to the PIC.
;   Registers are then restored and iret returns from the interrupt.
;
; WHY NOT REUSE isr_common:
;   isr_common calls isr_common_handler() which always halts. IRQ handlers
;   must return, so they need their own dispatch path. The register save/restore
;   sequence is the same, but the C function called differs.
;
; COUPLING NOTE:
;   irq_dispatch() is declared in isr.h and defined in isr.c.
; =============================================================================

extern irq_dispatch   ; C function in isr.c: dispatches to registered handler

%macro IRQ_STUB 1
global irq%1                ; make irqN visible so idt.c can take its address
irq%1:
    pusha               ; save all general-purpose registers (EAX...EDI)

    ; Save segment registers individually (no bulk push for these).
    push ds
    push es
    push fs
    push gs

    ; Load the kernel data segment into all data selectors.
    ; This is required in case the interrupt fired while user-mode code was
    ; running (future phase) where DS etc. would point to user segments.
    mov ax, 0x10        ; kernel data segment selector (GDT entry 2)
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Pass the IRQ number (0-15) to irq_dispatch().
    ; irq_dispatch calls the registered C handler and sends PIC EOI.
    push dword %1
    call irq_dispatch
    add  esp, 4         ; pop the irq number argument

    ; Restore segment registers in reverse push order.
    pop gs
    pop fs
    pop es
    pop ds

    ; Restore all general-purpose registers (reverse of pusha).
    popa

    ; Return from interrupt: pops EIP, CS, EFLAGS (and SS, ESP on priv change).
    iret
%endmacro

; Instantiate IRQ stubs for all 16 hardware IRQ lines (0-15).
; Each maps to an IDT vector that is 32 higher: IRQ0 -> vector 32, etc.
IRQ_STUB 0    ; IRQ0  PIT timer tick        (vector 32)
IRQ_STUB 1    ; IRQ1  PS/2 keyboard         (vector 33)
IRQ_STUB 2    ; IRQ2  Cascade to slave PIC  (vector 34) -- not a real device
IRQ_STUB 3    ; IRQ3  COM2 serial port      (vector 35)
IRQ_STUB 4    ; IRQ4  COM1 serial port      (vector 36)
IRQ_STUB 5    ; IRQ5  LPT2 / sound card     (vector 37)
IRQ_STUB 6    ; IRQ6  Floppy disk           (vector 38)
IRQ_STUB 7    ; IRQ7  LPT1 / spurious       (vector 39)
IRQ_STUB 8    ; IRQ8  CMOS real-time clock  (vector 40)
IRQ_STUB 9    ; IRQ9  Free / ACPI           (vector 41)
IRQ_STUB 10   ; IRQ10 Free                  (vector 42)
IRQ_STUB 11   ; IRQ11 Free                  (vector 43)
IRQ_STUB 12   ; IRQ12 PS/2 mouse            (vector 44)
IRQ_STUB 13   ; IRQ13 FPU / coprocessor     (vector 45)
IRQ_STUB 14   ; IRQ14 Primary ATA disk      (vector 46)
IRQ_STUB 15   ; IRQ15 Secondary ATA / spurious (vector 47)

; =============================================================================
; GNU STACK NOTE -- suppress "executable stack" linker warning.
; NASM does not emit this section by default, but modern linkers expect it.
; =============================================================================
section .note.GNU-stack noalloc noexec nowrite progbits
