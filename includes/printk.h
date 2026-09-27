#ifndef PRINTK_H
#define PRINTK_H

/* Supported: %c %s %d %i %u %x %X %p %%, with optional '0' pad and width
   (e.g. %08x). Returns the number of characters printed. */
int printk(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#endif
