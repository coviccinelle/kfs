#include "shell.h"
#include "printk.h"
#include "terminal.h"
#include "string.h"
#include "stack.h"
#include "ports.h"
#include "gdt.h"
#include "idt.h"
#include "panic.h"
#include "signal.h"
#include "syscall.h"
#include "timer.h"

#define SHELL_BUFSIZE 256

static char line[SHELL_BUFSIZE];
static size_t len = 0;
static int running = 0;     /* a command is executing */

static void prompt(void)
{
    printk("kfs> ");
}

/* Print the prompt and the half-typed line again after async output. */
static void redraw(void)
{
    prompt();
    for (size_t i = 0; i < len; i++)
        terminal_putchar(line[i]);
}

static void cmd_help(void)
{
    printk("available commands:\n");
    printk("  help   - show this list\n");
    printk("  echo   - print text back\n");
    printk("  duckie - dancing duck :)\n");
    printk("  stack  - dump the kernel stack\n");
    printk("  gdt    - show the global descriptor table\n");
    printk("  idt    - show the interrupt descriptor table\n");
    printk("  uptime - timer ticks since boot\n");
    printk("  kill N - schedule signal N (2 INT,10 USR1,14 ALRM,15 TERM)\n");
    printk("  alarm S [N] - schedule signal N (default 14) in S seconds\n");
    printk("  syscall - write through int 0x80\n");
    printk("  int3 / div0 / gpf / ud2 / into - trigger an exception\n");
    printk("  int N  - raise software interrupt N (0-255, 0x hex ok)\n");
    printk("  panic  - kernel panic (stack saved, registers cleaned)\n");
    printk("  clear  - clear the screen\n");
    printk("  reboot - restart the machine\n");
    printk("  halt   - stop the CPU\n");
    printk("  Ctrl+C sends SIGINT\n");
}

static void cmd_reboot(void)
{
    uint8_t status = 0x02;

    __asm__ volatile ("cli");
    while (status & 0x02)          /* wait input buffer of 8042 empty */
        status = inb(0x64);
    outb(0x64, 0xFE);              /* signal reset CPU */
    __asm__ volatile ("hlt");
}

static void cmd_duckie(void)
{
    const char *frame_a =
        "\n"
        "     _\n"
        "   >(o )\n"
        "    (  )>\n"
        "    ~~~~\n";
    const char *frame_b =
        "\n"
        "       _\n"
        "    ( o)<\n"
        "   <(  )\n"
        "    ~~~~\n";

    for (int i = 0; i < 8; i++) {
        terminal_clear();
        printk("%s", (i % 2 == 0) ? frame_a : frame_b);
        timer_sleep(TIMER_HZ / 2);
    }
    terminal_clear();
    printk("quack quack! :)\n");
}

/* return 1 if 's' starting wiht 'prefix' */
static int starts_with(const char *s, const char *prefix)
{
    while (*prefix) {
        if (*s != *prefix)
            return 0;
        s++;
        prefix++;
    }
    return 1;
}

static int hex_digit(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* Parse an unsigned number, decimal or 0x-prefixed hex, move *s past it.
   -1 if none or if it does not fit in 16 bits. */
static int parse_uint(const char **s)
{
    int n = 0;

    while (**s == ' ')
        (*s)++;
    if ((*s)[0] == '0' && ((*s)[1] == 'x' || (*s)[1] == 'X')
        && hex_digit((*s)[2]) >= 0) {
        *s += 2;
        while (hex_digit(**s) >= 0) {
            n = n * 16 + hex_digit(*(*s)++);
            if (n > 0xFFFF)
                return -1;
        }
        return n;
    }
    if (**s < '0' || **s > '9')
        return -1;
    while (**s >= '0' && **s <= '9') {
        n = n * 10 + (*(*s)++ - '0');
        if (n > 0xFFFF)
            return -1;
    }
    return n;
}

static void cmd_kill(const char *arg)
{
    int sig = parse_uint(&arg);

    if (signal_schedule(sig) < 0)
        printk("usage: kill <1-31>\n");
    else
        printk("%s scheduled\n", signal_name(sig));
}

static void cmd_alarm(const char *arg)
{
    int secs = parse_uint(&arg);
    int sig = parse_uint(&arg);

    if (sig < 0)
        sig = SIGALRM;
    if (secs < 0 || signal_schedule_in(sig, (uint32_t)secs * TIMER_HZ) < 0)
        printk("usage: alarm <seconds> [signal]\n");
    else
        printk("%s in %d s\n", signal_name(sig), secs);
}

static void cmd_syscall(void)
{
    static const char msg[] = "hello from int 0x80\n";
    uint32_t ret;

    __asm__ volatile ("int $0x80"
                      : "=a"(ret)
                      : "a"(SYS_WRITE), "b"(msg), "c"(sizeof msg - 1)
                      : "memory");
    printk("sys_write returned %u\n", ret);
    __asm__ volatile ("int $0x80" : "=a"(ret) : "a"(SYS_TICKS));
    printk("sys_ticks returned %u\n", ret);
}

static void cmd_div0(void)
{
    volatile int zero = 0;
    volatile int result = 42 / zero;   /* #DE, vector 0x00 */
    (void)result;
}

static void cmd_gpf(void)
{
    /* Loading a selector past the GDT limit raises #GP, vector 0x0D. */
    __asm__ volatile ("movw $0x80, %%ax; movw %%ax, %%ds" : : : "ax");
}

static void cmd_ud2(void)
{
    __asm__ volatile ("ud2");           /* #UD, vector 0x06 */
}

static void cmd_into(void)
{
    /* 0x7fffffff + 1 overflows a signed int: OF=1, so into raises #OF. */
    __asm__ volatile ("movl $0x7fffffff, %%eax; addl $1, %%eax; into"
                      : : : "eax", "cc");
}

/* "int $n; ret" x 256, 4 bytes each (isr_stubs.s) */
extern void int_trigger(void);

/*
Raise any software interrupt. Vectors where the CPU pushes an error code
are refused: a software int never pushes one, so the stub would read
a wrong stack layout (a known x86 limitation, not a kernel bug).
*/
static void cmd_int(const char *arg)
{
    int n = parse_uint(&arg);

    if (n < 0 || n > 255) {
        printk("usage: int <0-255> (decimal or 0x hex)\n");
        return;
    }
    if (n == 8 || (n >= 10 && n <= 14) || n == 17 || n == 21
        || n == 29 || n == 30) {
        printk("vector 0x%02x expects a CPU error code; use div0, gpf, "
               "ud2 or into to raise a real exception\n", n);
        return;
    }
    printk("int 0x%02x\n", n);
    ((void (*)(void))((uint32_t)int_trigger + (uint32_t)n * 4))();
    printk("back from int 0x%02x\n", n);
}

void shell_run_command(const char *cmd)
{
    if (cmd[0] == '\0')              return;   /* ignore empty line */
    else if (!strcmp(cmd, "help"))   cmd_help();
    else if (!strcmp(cmd, "stack"))  print_kernel_stack();
    else if (!strcmp(cmd, "gdt"))    gdt_print();
    else if (!strcmp(cmd, "idt"))    idt_print();
    else if (!strcmp(cmd, "uptime"))
        printk("%u ticks (%u s)\n", timer_ticks(), timer_ticks() / TIMER_HZ);
    else if (starts_with(cmd, "kill "))  cmd_kill(cmd + 5);
    else if (starts_with(cmd, "alarm ")) cmd_alarm(cmd + 6);
    else if (!strcmp(cmd, "syscall")) cmd_syscall();
    else if (!strcmp(cmd, "int3"))   __asm__ volatile ("int3");
    else if (!strcmp(cmd, "div0"))   cmd_div0();
    else if (!strcmp(cmd, "gpf"))    cmd_gpf();
    else if (!strcmp(cmd, "ud2"))    cmd_ud2();
    else if (!strcmp(cmd, "into"))   cmd_into();
    else if (starts_with(cmd, "int ")) cmd_int(cmd + 4);
    else if (!strcmp(cmd, "panic"))  panic("panic requested from the shell");
    else if (!strcmp(cmd, "clear"))  terminal_clear();
    else if (!strcmp(cmd, "reboot")) cmd_reboot();
    else if (!strcmp(cmd, "halt"))   system_halt("halted.");
    else if (!strcmp(cmd, "duckie")) cmd_duckie();
    else if (starts_with(cmd, "echo") && (cmd[4] == ' ' || cmd[4] == '\0')) {
        const char *arg = cmd + 4;         /* skip "echo" */
        while (*arg == ' ')                /* skip empty chars */
            arg++;
        printk("%s\n", arg);
    }
    else printk("unknown command: %s\n", cmd);
}

/* Ctrl+C: drop the current line, like a Unix shell. */
static void on_sigint(int sig)
{
    (void)sig;
    printk("^C\n");
    len = 0;
    prompt();
}

/* Signals the shell chooses to catch just print a message. */
static void on_signal(int sig)
{
    printk("\n[signal] caught %s (%d)\n", signal_name(sig), sig);
    if (!running)
        redraw();
}

void shell_init(void)
{
    len = 0;
    signal(SIGINT, on_sigint);
    signal(SIGALRM, on_signal);
    signal(SIGUSR1, on_signal);
    signal(SIGUSR2, on_signal);
    signal(SIGTRAP, on_signal);
    prompt();
}

/* receive input chars, stock in buffer, Enter -> execute */
void shell_putchar(char c)
{
    if (c == '\n') {
        line[len] = '\0';
        terminal_putchar('\n');
        running = 1;
        shell_run_command(line);
        running = 0;
        len = 0;
        prompt();
    } else if (c == '\b') {
        if (len > 0) {
            len--;
            terminal_putchar('\b');   /* clear on screen + buffer */
        }
    } else if (len < SHELL_BUFSIZE - 1) {
        line[len++] = c;
        terminal_putchar(c);          /* echo char on terminal */
    }
}
