# DugOS -- kernel build.
# Produces dugos.elf (multiboot) and dugos.iso (GRUB-bootable).

CC      := gcc
LD      := ld
ASM     := nasm

CFLAGS  := -m32 -ffreestanding -nostdlib -fno-pie -fno-stack-protector \
           -fno-asynchronous-unwind-tables -Wall -Wextra -std=gnu99
LDFLAGS := -m elf_i386 -T linker.ld -nostdlib

# C source files -- add new modules here as each phase is implemented.
C_SRCS   := main.c vga.c gdt.c idt.c isr.c \
             pic.c keyboard.c string.c \
             fs.c dir.c shell.c

# Assembly source files -- boot entry and all ISR/IRQ stubs.
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

# Per-object header dependencies.
# Each line lists the headers that the corresponding .c file #includes.
# If a header changes, make will recompile only the affected objects.
main.o:     vga.h gdt.h idt.h pic.h keyboard.h fs.h dir.h shell.h string.h
vga.o:      vga.h
gdt.o:      gdt.h
idt.o:      idt.h
isr.o:      isr.h vga.h pic.h
pic.o:      pic.h port.h vga.h
keyboard.o: keyboard.h isr.h pic.h port.h
string.o:   string.h
fs.o:       fs.h vga.h string.h keyboard.h
dir.o:      dir.h fs.h string.h vga.h keyboard.h
shell.o:    shell.h vga.h keyboard.h string.h fs.h dir.h pic.h port.h

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
