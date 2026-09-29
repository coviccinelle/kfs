#ifndef STACK_H
#define STACK_H

#include <stdint.h>

#define KERNEL_STACK_SIZE 16384     /* must match boot.s */

/* Copy of the kernel stack taken right before a panic or halt. */
struct stack_snapshot {
    uint32_t esp;          /* address the copy starts at */
    uint32_t ebp;          /* frame pointer at save time */
    uint32_t size;         /* bytes copied (esp .. stack_top) */
    uint8_t  data[KERNEL_STACK_SIZE];
};

void print_kernel_stack(void);
void print_stack_trace(uint32_t ebp, uint32_t esp, int max_depth);
const char *stack_symbol(uint32_t address, uint32_t *offset);
const struct stack_snapshot *stack_save(uint32_t esp, uint32_t ebp);
const struct stack_snapshot *stack_last_saved(void);

#endif
