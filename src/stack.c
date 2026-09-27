#include "stdint.h"
#include "stack.h"
#include "printk.h"

/* boot.s: the 16 KiB kernel stack (grows down from stack_top) */
extern uint32_t stack_bottom[];
extern uint32_t stack_top[];

#define DUMP_WORDS  24
#define MAX_FRAMES  16

/*
 * Every function compiled with a frame pointer (-fno-omit-frame-pointer)
 * starts with
 *     push %ebp ; mov %esp, %ebp
 * so [ebp] holds the caller's saved EBP and [ebp + 4] the return address.
 * Following [ebp] walks back through the callers until the EBP = 0 set in
 * _start.
 *
 *   higher addresses   | ...            |
 *                      | return address | <- ebp + 4
 *   ebp -------------> | caller's ebp   | ---> next frame
 *                      | locals ...     |
 *   esp -------------> | ...            |
 */
void print_kernel_stack(void)
{
    uint32_t *esp, *ebp;
    uint16_t ss;

    __asm__ volatile ("mov %%esp, %0" : "=r"(esp));
    __asm__ volatile ("mov %%ebp, %0" : "=r"(ebp));
    __asm__ volatile ("mov %%ss, %0" : "=r"(ss));

    printk("kernel stack: SS=%02x ESP=%p EBP=%p\n", ss, (void *)esp, (void *)ebp);
    printk("  range %p-%p, %u/%u bytes used\n", (void *)stack_bottom, (void *)stack_top,
           (uint32_t)stack_top - (uint32_t)esp, (uint32_t)stack_top - (uint32_t)stack_bottom);

    /* raw dump from ESP upwards, 4 words per line, never past stack_top */
    for (uint32_t *p = esp; p < esp + DUMP_WORDS && p < stack_top; p += 4) {
        printk("  %p:", (void *)p);
        for (int i = 0; i < 4 && p + i < stack_top; i++)
            printk(" %08x", p[i]);
        printk("\n");
    }

    printk("call trace:\n");
    for (int depth = 0; ebp && depth < MAX_FRAMES; depth++) {
        /* a corrupted EBP must not make us read outside the stack */
        if (ebp < esp || ebp + 1 >= stack_top) {
            printk("  #%d ebp=%p outside the stack, stopping\n", depth, (void *)ebp);
            break;
        }
        printk("  #%d ebp=%p ret=%p\n", depth, (void *)ebp, (void *)ebp[1]);
        ebp = (uint32_t *)ebp[0];
    }
}
