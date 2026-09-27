#ifndef GDT_H
#define GDT_H

#include "stdint.h"

#define GDT_ENTRIES 7

/* selector = index << 3 | TI (0 = GDT) << 2 | RPL */
#define GDT_KERNEL_CODE  0x08
#define GDT_KERNEL_DATA  0x10
#define GDT_KERNEL_STACK 0x18
#define GDT_USER_CODE    (0x20 | 3)
#define GDT_USER_DATA    (0x28 | 3)
#define GDT_USER_STACK   (0x30 | 3)

/* access byte: P | DPL(2) | S | E | DC | RW | A */
#define GDT_ACCESS_KERNEL_CODE 0x9A /* present, ring 0, code, readable */
#define GDT_ACCESS_KERNEL_DATA 0x92 /* present, ring 0, data, writable */
#define GDT_ACCESS_USER_CODE   0xFA /* present, ring 3, code, readable */
#define GDT_ACCESS_USER_DATA   0xF2 /* present, ring 3, data, writable */

/* flags nibble: G (4 KiB granularity) | D/B (32-bit) | L | AVL */
#define GDT_FLAGS_32BIT_4K 0xC0

typedef struct __attribute__((packed)) {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  granularity; /* flags (high nibble) | limit 19:16 (low nibble) */
    uint8_t  base_high;
} gdt_entry_t;

typedef struct __attribute__((packed)) {
    uint16_t limit;
    uint32_t base;
} gdt_ptr_t;

void gdt_init(void);
void gdt_print(void);

#endif // GDT_H
