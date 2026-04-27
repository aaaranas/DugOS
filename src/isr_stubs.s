; DugOS -- ISR stubs and GDT/IDT load helpers.
;
; Each ISR pushes its vector number and a (real or dummy) error code,
; then dispatches to a common stub that saves CPU state and calls the
; C handler. Vectors 8, 10-14, 17, 30 push a real error code; the
; others get a dummy 0 so struct regs is always the same shape.

bits 32

; ---- gdt_flush(uint32_t gdt_descriptor_addr) -------------------
global gdt_flush
gdt_flush:
    mov eax, [esp + 4]
    lgdt [eax]

    mov ax, 0x10            ; kernel data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    jmp 0x08:.flush         ; far jump reloads CS to kernel code segment
.flush:
    ret

; ---- idt_flush(uint32_t idt_descriptor_addr) -------------------
global idt_flush
idt_flush:
    mov eax, [esp + 4]
    lidt [eax]
    ret

; ---- ISR macros ------------------------------------------------
%macro ISR_NOERR 1
global isr%1
isr%1:
    cli
    push dword 0            ; dummy error code
    push dword %1           ; vector number
    jmp isr_common
%endmacro

%macro ISR_ERR 1
global isr%1
isr%1:
    cli
    ; CPU already pushed real error code
    push dword %1           ; vector number
    jmp isr_common
%endmacro

ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_NOERR 21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_ERR   30
ISR_NOERR 31

; ---- Common ISR stub -------------------------------------------
extern isr_common_handler

isr_common:
    pusha                   ; push edi,esi,ebp,esp,ebx,edx,ecx,eax
    push ds
    push es
    push fs
    push gs

    mov ax, 0x10            ; ensure kernel data segments
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp                ; argument: pointer to struct regs
    call isr_common_handler
    add esp, 4

    pop gs
    pop fs
    pop es
    pop ds
    popa
    add esp, 8              ; drop vector + err_code
    iret

; Mark the stack as non-executable for the linker.
section .note.GNU-stack noalloc noexec nowrite progbits
