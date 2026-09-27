#include "terminal.h"
#include "keyboard.h"
#include "gdt.h"
#include "printk.h"
#include "stack.h"
#include "shell.h"

/* The kernel only runs on 32-bit x86 (i386). */
#if !defined(__i386__)
#error "This kernel must be compiled for i386 (cross compiler or gcc -m32)"
#endif

#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002

void kernel_main(uint32_t magic, uint32_t mbi);

void kernel_main(uint32_t magic, uint32_t mbi)
{
  gdt_init();
  terminal_initialize();

  if (magic != MULTIBOOT_BOOTLOADER_MAGIC)
    printk("warning: bad multiboot magic %08x\n", magic);
  printk("KFS_2 booted, multiboot info at %p\n", (void *)mbi);

  gdt_print();
  print_kernel_stack();
  shell_init();

  while (1)
  {
    poll_keyboard();
  }
}
