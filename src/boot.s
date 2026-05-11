; =============================================================================
; boot.s -- DugOS multiboot entry point
;
; PURPOSE:
;   This is the very first code the CPU executes after GRUB loads the kernel.
;   It provides the Multiboot 1 header so GRUB can identify and load the
;   kernel, sets up an initial stack in memory, then transfers control to
;   the C kernel entry point (kmain) in main.c.
;
; HOW IT WORKS:
;   GRUB reads the first 8 KiB of the kernel ELF looking for the Multiboot
;   magic number (0x1BADB002). When found, GRUB loads the kernel, sets the
;   CPU to 32-bit protected mode, and jumps to _start below.
;
; REFERENCED BY: linker.ld (places .multiboot section first in the image)
; CALLS INTO:    kmain() in src/main.c
; =============================================================================

bits 32     ; All code in this file is 32-bit protected mode

; =============================================================================
; MULTIBOOT 1 HEADER
;
; The Multiboot specification requires a 12-byte magic header somewhere in
; the first 8 KiB of the kernel binary. GRUB searches for it.
;
;   MAGIC    -- A fixed constant (0x1BADB002) that GRUB recognises.
;   FLAGS    -- Tells GRUB what features we need:
;                 Bit 0 (0x01) = align loaded modules on 4 KiB page boundaries
;                 Bit 1 (0x02) = provide a memory map in the boot info struct
;   CHECKSUM -- Must make MAGIC + FLAGS + CHECKSUM == 0 (mod 2^32).
;
; We place this in its own section (.multiboot) so linker.ld can guarantee
; it appears at the very start of the output binary.
; =============================================================================
MAGIC    equ 0x1BADB002
FLAGS    equ 0x00000003          ; request module alignment + memory map
CHECKSUM equ -(MAGIC + FLAGS)    ; two's complement so the three sum to 0

section .multiboot
align 4                          ; GRUB requires the header to be 4-byte aligned
    dd MAGIC                     ; 4 bytes: magic number
    dd FLAGS                     ; 4 bytes: feature flags
    dd CHECKSUM                  ; 4 bytes: checksum (makes total = 0)

; =============================================================================
; KERNEL STACK
;
; C code (kmain and everything it calls) needs a valid stack before it can
; run -- local variables, function arguments, and return addresses all live
; on the stack. GRUB does NOT set one up for us, so we must do it here.
;
; We reserve 16 KiB (16384 bytes) in the BSS segment. BSS is zero-initialised
; by GRUB on load, so no explicit clearing is needed. The stack grows
; DOWNWARD on x86, so we point ESP at the TOP (highest address) of the block.
; =============================================================================
section .bss
align 16                ; align to 16 bytes (required by System V ABI for SSE)
stack_bottom:
    resb 16384          ; reserve 16 KiB -- enough for the kernel boot sequence
stack_top:              ; ESP will point here; stack grows toward stack_bottom

; =============================================================================
; KERNEL ENTRY POINT (_start)
;
; GRUB jumps here in 32-bit protected mode with:
;   EAX = 0x2BADB002  (Multiboot magic, confirms we were booted by GRUB)
;   EBX = physical address of the Multiboot information structure
;   Interrupts are DISABLED (IF flag is clear)
;   A GDT is set up by GRUB but we will replace it in gdt_init()
;
; We must NOT return from kmain. If kmain ever returns (it shouldn't),
; the .hang loop below halts the CPU so we don't execute random memory.
; =============================================================================
section .text
global _start           ; make _start visible to the linker
extern kmain            ; kmain is defined in src/main.c

_start:
    mov esp, stack_top  ; point the stack pointer to the top of our 16 KiB stack
    cli                 ; disable hardware interrupts -- we have no IDT yet,
                        ;   so any interrupt would triple-fault the CPU
    call kmain          ; transfer control to the C kernel entry point

.hang:
    ; kmain should never return because it contains an infinite loop.
    ; If it does return (e.g. during debugging), safely halt here.
    hlt                 ; halt the CPU (stops execution until next interrupt)
    jmp .hang           ; if an NMI wakes the CPU, halt again

; =============================================================================
; GNU STACK NOTE
;
; Modern linkers warn if any object file lacks this section, because its
; absence implies the stack may need to be executable. For a kernel this
; does not matter (we control the page tables), but we add it to suppress
; the warning and follow best practice.
; =============================================================================
section .note.GNU-stack noalloc noexec nowrite progbits
