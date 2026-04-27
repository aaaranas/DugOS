; DugOS -- multiboot entry point.
; Provides a 32-bit stack and hands control to kmain (in main.c).

bits 32

; ---- Multiboot 1 header ----------------------------------------
MAGIC    equ 0x1BADB002
FLAGS    equ 0x00000003          ; ALIGN_MODULES | MEMINFO
CHECKSUM equ -(MAGIC + FLAGS)

section .multiboot
align 4
    dd MAGIC
    dd FLAGS
    dd CHECKSUM

; ---- Stack (BSS, 16 KiB) ---------------------------------------
section .bss
align 16
stack_bottom:
    resb 16384
stack_top:

; ---- Entry point -----------------------------------------------
section .text
global _start
extern kmain

_start:
    mov esp, stack_top          ; ESP = top of our stack
    cli                          ; interrupts off until we install an IDT
    call kmain                   ; jump into C
.hang:
    hlt
    jmp .hang
