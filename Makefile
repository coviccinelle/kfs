# Use the i386 cross toolchain if installed (`make TARGET=i686-elf` to pick
# another prefix), otherwise the host gcc/as/ld in 32-bit mode: the flags
# below keep the kernel free of any host header or library.
TARGET   ?= i386-elf
ifneq ($(shell command -v $(TARGET)-gcc 2>/dev/null),)
CC       := $(TARGET)-gcc
AS       := $(TARGET)-as
LD       := $(TARGET)-ld
else
CC       := gcc -m32
AS       := as --32
LD       := ld -m elf_i386
endif

# grub-mkrescue (Debian/Ubuntu) or grub2-mkrescue (Fedora/Arch)
MKRESCUE ?= $(shell command -v grub-mkrescue 2>/dev/null || command -v grub2-mkrescue 2>/dev/null)
GRUB_DIR ?= /usr/lib/grub/i386-pc

SRCFILES := $(wildcard src/*.c)
HDRFILES := $(wildcard includes/*.h)
OBJFILES := $(patsubst src/%.c,%.o,$(SRCFILES))
DEPFILES := $(OBJFILES:.o=.d)

KERNEL := myos.bin
ISO    := kfs.iso

WARNINGS := -Wall -Wextra -pedantic -Wshadow -Wpointer-arith -Wcast-align \
            -Wwrite-strings -Wmissing-prototypes -Wmissing-declarations \
            -Wredundant-decls -Wnested-externs -Winline -Wno-long-long \
            -Wconversion -Wstrict-prototypes

# Freestanding kernel: no host headers/libs, no builtins, no stack canary,
# and keep EBP as frame pointer so the stack tracer can walk the frames.
# -fno-pie: host compilers build position-independent code by default,
# a kernel loaded at a fixed address must not.
CFLAGS := -I ./includes/ -g -std=gnu99 -O2 -march=i386 -ffreestanding \
          -fno-builtin -fno-stack-protector -fno-exceptions -fno-pie \
          -fno-omit-frame-pointer -fno-asynchronous-unwind-tables \
          -nostdlib -nodefaultlibs -MMD $(WARNINGS)

# ld called directly: only our objects and our linker script, nothing else
LDFLAGS := -T linker.ld -nostdlib

# BIOS-only GRUB image with the two modules we need: keeps the ISO < 1 MB
GRUB_FLAGS := -d $(GRUB_DIR) --install-modules="multiboot normal" \
              --fonts="" --locales="" --themes=""

all: $(ISO)

$(KERNEL): boot.o $(OBJFILES) linker.ld
	$(LD) $(LDFLAGS) -o $@ boot.o $(OBJFILES)

%.o: src/%.c Makefile
	$(CC) $(CFLAGS) -c $< -o $@

boot.o: boot.s
	$(AS) $< -o $@

$(ISO): $(KERNEL) grub.cfg
	mkdir -p isodir/boot/grub
	cp $(KERNEL) isodir/boot/$(KERNEL)
	cp grub.cfg isodir/boot/grub/grub.cfg
	$(MKRESCUE) $(GRUB_FLAGS) -o $@ isodir

clean:
	$(RM) boot.o $(OBJFILES) $(DEPFILES)
	$(RM) -r isodir

fclean: clean
	$(RM) $(KERNEL) $(ISO)

re: fclean all

run: $(ISO)
	qemu-system-i386 -cdrom $(ISO) -monitor stdio

run-kernel: $(KERNEL)
	qemu-system-i386 -kernel $(KERNEL) -monitor stdio

debug: $(ISO)
	qemu-system-i386 -cdrom $(ISO) -s -S -monitor stdio

-include $(DEPFILES)

.PHONY: all clean fclean re run run-kernel debug
