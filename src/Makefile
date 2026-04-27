# DugOS -- kernel build.
# Produces dugos.elf (multiboot) and dugos.iso (GRUB-bootable).

CC      := gcc
LD      := ld
ASM     := nasm

CFLAGS  := -m32 -ffreestanding -nostdlib -fno-pie -fno-stack-protector \
           -fno-asynchronous-unwind-tables -Wall -Wextra -std=gnu99
LDFLAGS := -m elf_i386 -T linker.ld -nostdlib

C_SRCS   := main.c vga.c gdt.c idt.c isr.c
ASM_SRCS := boot.s isr_stubs.s
OBJS     := $(ASM_SRCS:.s=.o) $(C_SRCS:.c=.o)

KERNEL := dugos.elf
ISO    := dugos.iso
ISODIR := iso

.PHONY: all iso run clean

all: $(KERNEL)

$(KERNEL): $(OBJS) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.s
	$(ASM) -f elf32 $< -o $@

# Per-object header dependencies
main.o: vga.h gdt.h idt.h
vga.o:  vga.h
gdt.o:  gdt.h
idt.o:  idt.h
isr.o:  isr.h vga.h

iso: $(KERNEL) grub.cfg
	rm -rf $(ISODIR)
	mkdir -p $(ISODIR)/boot/grub
	cp $(KERNEL) $(ISODIR)/boot/$(KERNEL)
	cp grub.cfg $(ISODIR)/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO) $(ISODIR) 2>/dev/null

run: iso
	qemu-system-i386 -cdrom $(ISO) -boot d

clean:
	rm -f $(OBJS) $(KERNEL) $(ISO)
	rm -rf $(ISODIR)
