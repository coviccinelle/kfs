#include "idt.h"
#include "pic.h"
#include "panic.h"
#include "printk.h"
#include "signal.h"
#include "string.h"

/* 256 gates, all present: vectors without a C handler reach
   interrupt_dispatch() and are reported there. */
static struct idt_entry idt[IDT_ENTRIES] __attribute__((aligned(8)));
static struct idt_ptr idtp;

/* C callbacks, one per vector, registered through the kernel API. */
static isr_t handlers[IDT_ENTRIES];

/* Stub addresses exported by isr_stubs.s */
extern const uint32_t isr_stub_table[IDT_ENTRIES];

static const char *const exception_names[32] = {
    "Division by Zero", "Debug", "Non Maskable Interrupt", "Breakpoint",
    "Overflow", "Bound Range Exceeded", "Invalid Opcode",
    "Coprocessor not available", "Double Fault",
    "Coprocessor Segment Overrun", "Invalid TSS", "Segment Not Present",
    "Stack Fault", "General Protection Fault", "Page Fault", "Reserved",
    "Math Fault", "Alignment Check", "Machine Check",
    "SIMD Floating-Point Exception", "Virtualization Exception",
    "Control Protection Exception", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Hypervisor Injection",
    "VMM Communication", "Security Exception", "Reserved",
};

const char *exception_name(uint32_t num)
{
    return num < 32 ? exception_names[num] : "Unknown";
}

void idt_set_gate(uint8_t num, uint32_t handler, uint16_t selector,
                  uint8_t type_attr)
{
    idt[num].offset_low  = (uint16_t)(handler & 0xFFFF);
    idt[num].offset_high = (uint16_t)((handler >> 16) & 0xFFFF);
    idt[num].selector    = selector;
    idt[num].zero        = 0;
    idt[num].type_attr   = type_attr;
}

void register_interrupt_handler(uint8_t num, isr_t handler)
{
    handlers[num] = handler;
}

/* #BP is a trap: the CPU resumes after int3, so it is safe to return.
   SIGTRAP is only scheduled: its callback runs later from the main loop,
   not inside the exception with interrupts off. */
static void breakpoint_handler(struct regs *r)
{
    printk("breakpoint at eip=%08x\n", r->eip);
    signal_schedule(SIGTRAP);
}

/* Every fault ends here unless a handler was registered for it. */
static void fault_handler(struct regs *r)
{
    panic_regs(exception_name(r->int_no), r);
}

/* Called by isr_common for every interrupt. */
void interrupt_dispatch(struct regs *r)
{
    uint32_t n = r->int_no;

    if (n >= IRQ(0) && n <= IRQ(15)) {
        uint8_t irq = (uint8_t)(n - IRQ_BASE);

        if (pic_is_spurious(irq))
            return;
        if (handlers[n])
            handlers[n](r);
        pic_send_eoi(irq);
    } else if (n < IDT_ENTRIES && handlers[n]) {
        handlers[n](r);
    } else if (n < 32) {
        fault_handler(r);
    } else {
        printk("unhandled interrupt 0x%02x\n", n);
    }
}

void idt_init(void)
{
    memset(idt, 0, sizeof idt);
    memset(handlers, 0, sizeof handlers);

    /* Every vector gets a stub: kernel only (DPL0), except int 0x80
       which may be called from ring 3 later (DPL3). */
    for (int i = 0; i < IDT_ENTRIES; i++)
        idt_set_gate((uint8_t)i, isr_stub_table[i], 0x08,
                     i == SYSCALL_VECTOR ? IDT_GATE_USER : IDT_GATE_KERNEL);

    /* Move IRQs away from the CPU exception vectors, mask them all. */
    pic_remap(IRQ(0), IRQ(8));

    register_interrupt_handler(3, breakpoint_handler);

    idtp.limit = (uint16_t)(sizeof idt - 1);
    idtp.base  = (uint32_t)idt;
    __asm__ volatile ("lidt %0" : : "m"(idtp));
}

void idt_print(void)
{
    struct idt_ptr r;
    int shown = 0;

    __asm__ volatile ("sidt %0" : "=m"(r));
    printk("IDTR base=%08x limit=%04x (%u gates)\n",
           r.base, r.limit, ((uint32_t)r.limit + 1) / 8);

    const struct idt_entry *e = (const struct idt_entry *)r.base;
    uint32_t count = ((uint32_t)r.limit + 1) / 8;
    uint32_t present = 0;
    for (uint32_t i = 0; i < count; i++)
        if (e[i].type_attr & 0x80)
            present++;
    printk("%u present gates, selector %04x\n", present, e[0].selector);

    /* 256 lines would not fit on screen: show the gates of interest, i.e.
       a few exceptions and every vector with a C handler. */
    printk("vec  offset    attr  handler\n");
    for (uint32_t i = 0; i < count; i++) {
        if (i != 0x00 && i != 0x08 && i != 0x0E && !handlers[i])
            continue;
        uint32_t off = e[i].offset_low | ((uint32_t)e[i].offset_high << 16);
        printk("%02x   %08x  %02x    %s\n", i, off, e[i].type_attr,
               handlers[i] ? "C callback" : "panic");
        shown++;
    }
    printk("(0x8E = DPL0 interrupt gate, 0xEE = DPL3; %d listed)\n", shown);
}
