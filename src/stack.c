#include "printk.h"
#include "stack.h"
#include "keyboard.h"
#include "shell.h"
#include "terminal.h"
#include "idt.h"
#include "panic.h"
#include "signal.h"
#include "string.h"

#include <stddef.h>

extern uint8_t stack_bottom[];
extern uint8_t stack_top[];
extern void _start(void);
extern void isr_common(void);
extern void int_trigger(void);
extern void cpu_halt_clean(void);
void kernel_main(void);

struct ksym {
    uint32_t addr;
    const char *name;
};

static const struct ksym ksyms[] = {
    { (uint32_t)_start, "_start" },
    { (uint32_t)kernel_main, "kernel_main" },
    { (uint32_t)keyboard_process, "keyboard_process" },
    { (uint32_t)handle_scancode, "handle_scancode" },
    { (uint32_t)shell_putchar, "shell_putchar" },
    { (uint32_t)shell_run_command, "shell_run_command" },
    { (uint32_t)print_kernel_stack, "print_kernel_stack" },
    { (uint32_t)isr_common, "isr_common" },
    { (uint32_t)interrupt_dispatch, "interrupt_dispatch" },
    { (uint32_t)panic, "panic" },
    { (uint32_t)cpu_halt_clean, "cpu_halt_clean" },
    { (uint32_t)int_trigger, "int_trigger" },
    { (uint32_t)signal_raise, "signal_raise" },
    { (uint32_t)signal_process, "signal_process" },
};

/* Last snapshot taken by stack_save(), kept for post-mortem. */
static struct stack_snapshot saved;

/* Name of the closest known function at or below address. */
const char *stack_symbol(uint32_t address, uint32_t *offset)
{
    const struct ksym *best = 0;

    for (size_t i = 0; i < sizeof ksyms / sizeof *ksyms; i++) {
        if (ksyms[i].addr <= address &&
            (!best || ksyms[i].addr > best->addr))
            best = &ksyms[i];
    }
    if (!best) {
        *offset = 0;
        return "??";
    }
    *offset = address - best->addr;
    return best->name;
}

struct stack_frame {
    struct stack_frame *ebp;
    uint32_t ret;
};

/* Walk the saved-EBP chain while it stays inside the kernel stack. */
void print_stack_trace(uint32_t ebp, uint32_t esp, int max_depth)
{
    uint32_t top = (uint32_t)stack_top;
    struct stack_frame *frame = (struct stack_frame *)ebp;

    for (int depth = 0; frame != 0 && (uint32_t)frame >= esp &&
         (uint32_t)frame + 8 <= top && depth < max_depth; depth++) {
        uint32_t offset;
        printk("  #%d %s+0x%x (ret %08x)\n", depth,
               stack_symbol(frame->ret, &offset), offset, frame->ret);
        frame = frame->ebp;
    }
}

/* Copy the live part of the stack (esp .. stack_top) somewhere safe. */
const struct stack_snapshot *stack_save(uint32_t esp, uint32_t ebp)
{
    uint32_t bottom = (uint32_t)stack_bottom;
    uint32_t top = (uint32_t)stack_top;

    if (esp < bottom || esp > top)
        esp = bottom;
    saved.esp = esp;
    saved.ebp = ebp;
    saved.size = top - esp;
    if (saved.size > KERNEL_STACK_SIZE)
        saved.size = KERNEL_STACK_SIZE;
    memcpy(saved.data, (const void *)esp, saved.size);
    return &saved;
}

const struct stack_snapshot *stack_last_saved(void)
{
    return saved.size ? &saved : 0;
}

void print_kernel_stack(void)
{
    uint32_t esp;
    uint32_t ebp;
    uint32_t bottom = (uint32_t)stack_bottom;
    uint32_t top = (uint32_t)stack_top;

    __asm__ volatile ("mov %%esp, %0" : "=r"(esp));
    __asm__ volatile ("mov %%ebp, %0" : "=r"(ebp));
    terminal_clear();

    uint32_t used = esp > top ? 0 : top - esp;
    printk("stack [%08x-%08x] used %u/%u bytes\n",
           bottom, top, used, top - bottom);
    printk("ESP=%08x EBP=%08x\n", esp, ebp);

    if (esp < bottom)
        esp = bottom;
    if (esp > top)
        esp = top;
    for (uint32_t address = esp; address < top; address += 16) {
        printk("%08x:", address);
        for (uint32_t word = address; word < address + 16 && word < top;
             word += 4)
            printk(" %08x%c", *(uint32_t *)word, word == ebp ? '*' : ' ');
        printk("\n");
    }

    printk("call trace:\n");
    print_stack_trace(ebp, esp, 16);
}
