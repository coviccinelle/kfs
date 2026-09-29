#include "panic.h"
#include "cpu.h"
#include "printk.h"
#include "stack.h"
#include "string.h"
#include "terminal.h"
#include "vga.h"

/* panic() itself is in isr_stubs.s: it captures the registers before any
   C code runs, then calls panic_regs() below. */

static void print_regs(const struct regs *r, uint32_t esp)
{
    printk("EAX=%08x EBX=%08x ECX=%08x EDX=%08x\n",
           r->eax, r->ebx, r->ecx, r->edx);
    printk("ESI=%08x EDI=%08x EBP=%08x ESP=%08x\n",
           r->esi, r->edi, r->ebp, esp);
    printk("EIP=%08x CS=%04x DS=%04x EFLAGS=%08x\n",
           r->eip, r->cs, r->ds, r->eflags);
}

static void print_snapshot(const struct stack_snapshot *s, int rows)
{
    printk("stack saved: %u bytes from %08x\n", s->size, s->esp);
    for (uint32_t off = 0; off < s->size && rows-- > 0; off += 16) {
        printk("%08x:", s->esp + off);
        for (uint32_t w = off; w < off + 16 && w < s->size; w += 4) {
            uint32_t v;
            memcpy(&v, s->data + w, sizeof v);
            printk(" %08x", v);
        }
        printk("\n");
    }
}

void panic_regs(const char *msg, const struct regs *r)
{
    __asm__ volatile ("cli");

    /* ESP of the interrupted code: right above what the CPU (or panic())
       pushed. Same layout in both cases, so the same formula works. */
    uint32_t esp = (uint32_t)&r->useresp;

    /* 1. Save the stack before anything else can change it. */
    const struct stack_snapshot *snap = stack_save(esp, r->ebp);

    /* 2. Report. */
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_RED));
    terminal_clear();
    printk("*** KERNEL PANIC: %s ***\n", msg);
    if (r->int_no < 32)
        printk("exception 0x%02x (%s) error code %08x\n",
               r->int_no, exception_name(r->int_no), r->err_code);
    print_regs(r, esp);
    print_snapshot(snap, 4);
    printk("call trace:\n");
    uint32_t offset;
    const char *where = stack_symbol(r->eip, &offset);
    printk("  -> %s+0x%x (eip %08x)\n", where, offset, r->eip);
    print_stack_trace(r->ebp, esp, 6);
    printk("registers cleaned, CPU halted.");

    /* 3. Clean registers and halt for good. */
    cpu_halt_clean();
}

void system_halt(const char *reason)
{
    uint32_t esp, ebp;

    __asm__ volatile ("cli");
    __asm__ volatile ("movl %%esp, %0; movl %%ebp, %1" : "=r"(esp), "=r"(ebp));
    const struct stack_snapshot *snap = stack_save(esp, ebp);

    printk("%s\n", reason);
    printk("stack saved (%u bytes at %08x), registers cleaned. bye.\n",
           snap->size, snap->esp);
    cpu_halt_clean();
}
