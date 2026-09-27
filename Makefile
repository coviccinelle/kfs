TARGET=i386-elf
CC := $(TARGET)-gcc
AS := $(TARGET)-as

PROJDIRS := src includes

SRCFILES := $(shell find $(PROJDIRS) -type f -name "*.c")
HDRFILES := $(shell find $(PROJDIRS) -type f -name "*.h")

OBJFILES := $(patsubst src/%,%, $(patsubst %.c,%.o, $(SRCFILES)))
DEPFILES := $(patsubst %.o,%.d,$(OBJFILES))

ALLFILES := $(SRCFILES) $(HDRFILES)

ISO := kfs.iso
# BIOS-only GRUB image (no EFI payload) keeps the ISO small
GRUB_MKRESCUE_FLAGS := -d /usr/lib/grub/i386-pc \
                       --install-modules="multiboot normal" \
                       --fonts="" --locales="" --themes=""

WARNINGS := -Wall -Wextra -pedantic -Wshadow -Wpointer-arith -Wcast-align \
            -Wwrite-strings -Wmissing-prototypes -Wmissing-declarations \
            -Wredundant-decls -Wnested-externs -Winline -Wno-long-long \
            -Wconversion -Wstrict-prototypes

CFLAGS := -I ./includes/ -g -ffreestanding -fno-builtin -fno-stack-protector \
          -fno-exceptions -fno-omit-frame-pointer -nostdlib -nodefaultlibs \
          -O2 -std=gnu99 -MMD $(WARNINGS)

LDFLAGS := -T linker.ld -ffreestanding -nostdlib -nodefaultlibs -O2

all: myos.bin $(ISO)

%.o: src/%.c Makefile
	$(CC) $(CFLAGS) -c $< -o $@

myos.bin: boot.o $(OBJFILES) linker.ld
	$(CC) $(LDFLAGS) -o myos.bin boot.o $(OBJFILES)

boot.o: boot.s
	$(AS) ./boot.s -o boot.o

$(ISO): myos.bin grub.cfg
	mkdir -p isodir/boot/grub
	cp myos.bin isodir/boot/myos.bin
	cp grub.cfg isodir/boot/grub/grub.cfg
	grub-mkrescue $(GRUB_MKRESCUE_FLAGS) -o $(ISO) isodir

iso: $(ISO)

clean:
	$(RM) boot.o $(OBJFILES) $(DEPFILES)
	$(RM) myos.bin
	$(RM) -r isodir

fclean: clean
	$(RM) $(ISO)

re: fclean all

start: myos.bin
	qemu-system-i386 -kernel myos.bin

start-iso: $(ISO)
	qemu-system-i386 -cdrom $(ISO)

todolist:
	-@for file in $(ALLFILES:Makefile=); do fgrep -H -e TODO -e FIXME $$file; done; true

-include $(DEPFILES)

.PHONY: all iso clean fclean re start start-iso todolist
