#include "gdt.h"
#include "printk.h"

/* 0x800, defined in linker.ld: the table is built directly at that address */
extern gdt_entry_t gdt_phys[GDT_ENTRIES];

/* boot.s: lgdt + reload of every segment register */
extern void gdt_flush(const gdt_ptr_t *ptr);

static gdt_ptr_t gdt_ptr;

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
    gdt_set_entry(0, 0, 0, 0, 0);                                                       /* null         */
    gdt_set_entry(1, 0, 0xFFFFFFFF, GDT_ACCESS_KERNEL_CODE, GDT_FLAGS_32BIT_4K);        /* kernel code  */
    gdt_set_entry(2, 0, 0xFFFFFFFF, GDT_ACCESS_KERNEL_DATA, GDT_FLAGS_32BIT_4K);        /* kernel data  */
    gdt_set_entry(3, 0, 0xFFFFFFFF, GDT_ACCESS_KERNEL_DATA, GDT_FLAGS_32BIT_4K);        /* kernel stack */
    gdt_set_entry(4, 0, 0xFFFFFFFF, GDT_ACCESS_USER_CODE,   GDT_FLAGS_32BIT_4K);        /* user code    */
    gdt_set_entry(5, 0, 0xFFFFFFFF, GDT_ACCESS_USER_DATA,   GDT_FLAGS_32BIT_4K);        /* user data    */
    gdt_set_entry(6, 0, 0xFFFFFFFF, GDT_ACCESS_USER_DATA,   GDT_FLAGS_32BIT_4K);        /* user stack   */

    gdt_ptr.limit = (uint16_t)(sizeof(gdt_entry_t) * GDT_ENTRIES - 1);
    gdt_ptr.base  = (uint32_t)gdt_phys;
    gdt_flush(&gdt_ptr);
}

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
    for (int i = 0; i < GDT_ENTRIES; i++) {
        const gdt_entry_t *e = &gdt_phys[i];
        uint32_t base  = (uint32_t)e->base_low | ((uint32_t)e->base_mid << 16)
                       | ((uint32_t)e->base_high << 24);
        uint32_t limit = (uint32_t)e->limit_low | ((uint32_t)(e->granularity & 0x0F) << 16);
        const char *type = !(e->access & 0x80) ? "-   "
                         : (e->access & 0x08) ? "code" : "data";

        printk(" %d %02x base=%08x lim=%05x acc=%02x fl=%x dpl=%d %s %s\n",
               i, i * 8 | ((e->access >> 5) & 3), base, limit, e->access,
               e->granularity >> 4, (e->access >> 5) & 3, type, names[i]);
    }
}
