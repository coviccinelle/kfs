#include "stdint.h"
#include "shell.h"
#include "terminal.h"
#include "printk.h"
#include "ports.h"
#include "keyboard.h"
#include "string.h"
#include "gdt.h"
#include "stack.h"

#define SHELL_LINE_MAX 128
#define SHELL_PROMPT   "kfs> "

static char line[SHELL_LINE_MAX];
static size_t line_len = 0;

static void cmd_help(void);
static void cmd_stack(void);
static void cmd_gdt(void);
static void cmd_clear(void);
static void cmd_reboot(void);
static void cmd_halt(void);

static const struct {
    const char *name;
    void (*fn)(void);
    const char *help;
} commands[] = {
    { "help",   cmd_help,   "list the commands" },
    { "stack",  cmd_stack,  "print the kernel stack and call trace" },
    { "gdt",    cmd_gdt,    "decode the GDT at 0x800" },
    { "clear",  cmd_clear,  "clear the screen" },
    { "reboot", cmd_reboot, "reset the machine" },
    { "halt",   cmd_halt,   "stop the machine" },
};

#define NB_COMMANDS (sizeof(commands) / sizeof(commands[0]))

static int str_eq(const char *a, const char *b)
{
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

static void cmd_help(void)
{
    for (size_t i = 0; i < NB_COMMANDS; i++) {
        printk("  %s", commands[i].name);
        for (size_t n = strlen(commands[i].name); n < 8; n++)
            terminal_putchar(' ');
        printk("%s\n", commands[i].help);
    }
}

static void cmd_stack(void)
{
    print_kernel_stack();
}

static void cmd_gdt(void)
{
    gdt_print();
}

static void cmd_clear(void)
{
    terminal_clear();
}

static void cmd_reboot(void)
{
    /* pulse the CPU reset line through the 8042 keyboard controller */
    while (inb(KEYBOARD_CTRL_PORT) & 0x02)
        ;
    outb(KEYBOARD_CTRL_PORT, 0xFE);

    /* fallback: an empty IDT turns the next interrupt into a triple fault */
    const struct __attribute__((packed)) { uint16_t limit; uint32_t base; } no_idt = { 0, 0 };
    __asm__ volatile ("lidt %0; int $3" : : "m"(no_idt));
}

static void cmd_halt(void)
{
    printk("System halted.\n");
    outw(0x604, 0x2000);  /* QEMU (q35/piix4 ACPI) power off */
    outw(0xB004, 0x2000); /* older QEMU / Bochs */
    __asm__ volatile ("cli");
    for (;;)
        __asm__ volatile ("hlt");
}

static void shell_prompt(void)
{
    terminal_writestring(SHELL_PROMPT);
}

static void shell_execute(void)
{
    line[line_len] = '\0';
    if (line_len == 0)
        return;
    for (size_t i = 0; i < NB_COMMANDS; i++) {
        if (str_eq(line, commands[i].name)) {
            commands[i].fn();
            return;
        }
    }
    printk("%s: command not found (try 'help')\n", line);
}

void shell_init(void)
{
    line_len = 0;
    printk("Type 'help' for the list of commands.\n");
    shell_prompt();
}

void shell_input(char c)
{
    if (c == '\n') {
        terminal_putchar('\n');
        shell_execute();
        line_len = 0;
        shell_prompt();
    } else if (c == '\b') {
        if (line_len > 0) { /* never erase the prompt */
            line_len--;
            terminal_putchar('\b');
        }
    } else if (c >= ' ' && c <= '~' && line_len < SHELL_LINE_MAX - 1) {
        line[line_len++] = c;
        terminal_putchar(c);
    }
}

void shell_on_screen_switch(void)
{
    /* the typed line belonged to the previous screen: drop it */
    line_len = 0;
    if (terminal.column != 0)
        terminal_putchar('\n');
    shell_prompt();
}
