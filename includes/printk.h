#ifndef PRINTK_H
#define PRINTK_H

/* Supported conversions: %c %s %d %u %x %p %%, optional '0' pad and width. */
void printk(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#endif // PRINTK_H
