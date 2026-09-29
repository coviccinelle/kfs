# Using Clang to compile, assemble and link the kernel, bootloader and tests. The target is i386-unknown-none-elf, which is a generic 32-bit x86 target without any OS or standard library. The compiler is instructed to use the i386 architecture.
CC := clang --target=i386-unknown-none-elf -march=i386
AS := as --32
LD := ld -m elf_i386

# Finding grub-mkrescue in PATH, or i386-elf-grub-mkrescue, or i686-elf-grub-mkrescue
GRUB_MKRESCUE := $(shell command -v grub-mkrescue 2>/dev/null || command -v grub2-mkrescue 2>/dev/null || command -v i386-elf-grub-mkrescue 2>/dev/null || command -v i686-elf-grub-mkrescue 2>/dev/null)
GRUB_BIOS_DIR := $(if $(wildcard .local/grub/usr/lib/grub/i386-pc),.local/grub/usr/lib/grub/i386-pc,/usr/lib/grub/i386-pc)

PROJDIRS := src includes

SRCFILES := $(shell find $(PROJDIRS) -type f -name "*.c")
HDRFILES := $(shell find $(PROJDIRS) -type f -name "*.h")

ASMFILES := $(shell find src -type f -name "*.s")

OBJFILES := $(patsubst src/%,%, $(patsubst %.c,%.o, $(SRCFILES))) \
            $(patsubst src/%.s,%.o, $(ASMFILES))
TSTFILES := $(patsubst %.c,%_t,$(SRCFILES))

DEPFILES    := $(patsubst %.o,%.d,$(OBJFILES))
TSTDEPFILES := $(patsubst %,%.d,$(TSTFILES))

ALLFILES := $(SRCFILES) $(HDRFILES) $(AUXFILES)

WARNINGS := -Wall -Wextra -pedantic -Wshadow -Wpointer-arith -Wcast-align \
            -Wwrite-strings -Wmissing-prototypes -Wmissing-declarations \
            -Wredundant-decls -Wnested-externs -Winline -Wno-long-long \
            -Wconversion -Wstrict-prototypes

CFLAGS := -I ./includes/ -g -O2 -std=gnu99 \
          -ffreestanding -fno-builtin -fno-stack-protector -fno-exceptions \
          -nostdlib -nodefaultlibs -fno-omit-frame-pointer -MMD -MP $(WARNINGS)

all: myos.iso

%.o: src/%.c Makefile
	$(CC) $(CFLAGS) -c $< -o $@

# interrupt stubs and other assembly in src/
%.o: src/%.s Makefile
	$(AS) $< -o $@

#  boot.s: using 'as --32' to assemble the bootloader
boot.o: boot.s
	$(AS) ./boot.s -o boot.o

# link: using LD directly to avoid dependency on external libgcc
myos.bin: boot.o $(OBJFILES) linker.ld
	$(LD) -T linker.ld -nostdlib boot.o $(OBJFILES) -o myos.bin

myos.iso: myos.bin grub.cfg
	@test -n "$(GRUB_MKRESCUE)" || (echo "grub-mkrescue introuvable" >&2; false)
	@test -d "$(GRUB_BIOS_DIR)" || (echo "GRUB BIOS modules missing: install grub2-pc-modules" >&2; false)
	mkdir -p isodir/boot/grub
	cp myos.bin isodir/boot/myos.bin
	cp grub.cfg isodir/boot/grub/grub.cfg
	$(GRUB_MKRESCUE) -d "$(GRUB_BIOS_DIR)" -o $@ \
		--compress=xz --fonts= --locales= --themes= \
		--install-modules="multiboot normal" isodir

clean:
	-@$(RM) $(wildcard $(OBJFILES) $(DEPFILES) $(TSTFILES) pdclib.a pdclib.tgz)
	$(RM) boot.o 
	$(RM) $(OBJFILES)
	$(RM) myos.bin
	$(RM) -r isodir

# myos.iso is the delivered image: only fclean removes it
fclean: clean
	$(RM) myos.iso

re: fclean all

start: myos.bin
	qemu-system-i386 -kernel myos.bin

start-iso: myos.iso
	qemu-system-i386 -boot d -cdrom myos.iso

todolist:
	-@for file in $(ALLFILES:Makefile=); do fgrep -H -e TODO -e FIXME $$file; done; true

-include $(DEPFILES)

.PHONY: all clean fclean re start start-iso