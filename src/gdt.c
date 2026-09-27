#include "gdt.h"
#include "printk.h"

/* 0x800, defined in linker.ld. Paging is off, so this address is physical:
   the table is written directly there, not copied from the kernel image. */
extern gdt_entry_t gdt_phys[GDT_ENTRIES];

/* boot.s: lgdt + reload of every segment register (far jump for CS) */
extern void gdt_flush(const gdt_ptr_t *ptr);

static gdt_ptr_t gdt_ptr;

/* fill one line of the table: spread base/limit/access over the 8 bytes */
static void gdt_set_entry(int i, uint32_t base, uint32_t limit, uint8_t access, uint8_t flags)
{
    gdt_phys[i].limit_low   = (uint16_t)(limit & 0xFFFF);
    gdt_phys[i].base_low    = (uint16_t)(base & 0xFFFF);
    gdt_phys[i].base_mid    = (uint8_t)((base >> 16) & 0xFF);
    gdt_phys[i].access      = access;
    gdt_phys[i].granularity = (uint8_t)((flags & 0xF0) | ((limit >> 16) & 0x0F));
    gdt_phys[i].base_high   = (uint8_t)((base >> 24) & 0xFF);
}

void gdt_init(void)
{
    /* flat model: every segment covers 0..4 GiB (limit 0xFFFFF x 4 KiB),
       only the privilege level (DPL) and the type (code/data) differ */
    gdt_set_entry(0, 0, 0, 0, 0);                                                  /* 0x00 null (mandatory) */
    gdt_set_entry(1, 0, 0xFFFFF, GDT_ACCESS_KERNEL_CODE, GDT_FLAGS_32BIT_4K);      /* 0x08 kernel code */
    gdt_set_entry(2, 0, 0xFFFFF, GDT_ACCESS_KERNEL_DATA, GDT_FLAGS_32BIT_4K);      /* 0x10 kernel data */
    gdt_set_entry(3, 0, 0xFFFFF, GDT_ACCESS_KERNEL_DATA, GDT_FLAGS_32BIT_4K);      /* 0x18 kernel stack */
    gdt_set_entry(4, 0, 0xFFFFF, GDT_ACCESS_USER_CODE,   GDT_FLAGS_32BIT_4K);      /* 0x20 user code */
    gdt_set_entry(5, 0, 0xFFFFF, GDT_ACCESS_USER_DATA,   GDT_FLAGS_32BIT_4K);      /* 0x28 user data */
    gdt_set_entry(6, 0, 0xFFFFF, GDT_ACCESS_USER_DATA,   GDT_FLAGS_32BIT_4K);      /* 0x30 user stack */

    gdt_ptr.limit = (uint16_t)(sizeof(gdt_entry_t) * GDT_ENTRIES - 1);
    gdt_ptr.base  = (uint32_t)gdt_phys;
    gdt_flush(&gdt_ptr);
}

/* decode the table actually loaded in GDTR (read back with sgdt) */
void gdt_print(void)
{
    static const char *names[GDT_ENTRIES] = {
        "null", "kernel code", "kernel data", "kernel stack",
        "user code", "user data", "user stack"
    };
    gdt_ptr_t cur;
    uint16_t cs, ds, ss;

    __asm__ volatile ("sgdt %0" : "=m"(cur));
    __asm__ volatile ("mov %%cs, %0" : "=r"(cs));
    __asm__ volatile ("mov %%ds, %0" : "=r"(ds));
    __asm__ volatile ("mov %%ss, %0" : "=r"(ss));

    printk("GDTR base=%p limit=%04x  CS=%02x DS=%02x SS=%02x\n",
           (void *)cur.base, cur.limit, cs, ds, ss);

    const gdt_entry_t *table = (const gdt_entry_t *)cur.base;
    for (int i = 0; i < GDT_ENTRIES; i++) {
        const gdt_entry_t *e = &table[i];
        uint32_t base  = (uint32_t)e->base_low | ((uint32_t)e->base_mid << 16)
                       | ((uint32_t)e->base_high << 24);
        uint32_t limit = (uint32_t)e->limit_low | ((uint32_t)(e->granularity & 0x0F) << 16);
        uint32_t dpl   = (uint32_t)(e->access >> 5) & 3;
        const char *type = !(e->access & 0x80) ? "-   "
                         : (e->access & 0x08) ? "code" : "data";

        printk(" %d sel=%02x base=%08x lim=%05x acc=%02x fl=%x dpl=%u %s %s\n",
               i, (uint32_t)(i * 8) | dpl, base, limit, e->access,
               (uint32_t)e->granularity >> 4, dpl, type, names[i]);
    }
}
