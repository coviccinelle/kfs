#include "terminal.h"
#include "keyboard.h"
#include "gdt.h"
#include "printk.h"
#include "stack.h"
#include "shell.h"

/* Check if the compiler thinks you are targeting the wrong operating system. */
#if defined(__linux__)
#error "You are not using a cross-compiler, you will most certainly run into trouble"
#endif

/* This tutorial will only work for the 32-bit ix86 targets. */
#if !defined(__i386__)
#error "This tutorial needs to be compiled with a ix86-elf compiler"
#endif

#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002

void kernel_main(uint32_t magic, uint32_t mbi);

void kernel_main(uint32_t magic, uint32_t mbi)
{
  gdt_init();
  terminal_initialize();

  if (magic != MULTIBOOT_BOOTLOADER_MAGIC)
    printk("warning: bad multiboot magic %08x\n", magic);
  printk("multiboot info at %p\n", (void *)mbi);

  gdt_print();
  print_kernel_stack();
  shell_init();

  while (1)
  {
    poll_keyboard();
  }
}
