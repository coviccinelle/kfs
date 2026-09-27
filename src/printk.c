#include <stdarg.h>
#include "stdint.h"
#include "printk.h"
#include "terminal.h"

static void print_num(uint32_t n, uint32_t base, int width, char pad)
{
    char buf[12];
    int i = 0;

    do {
        buf[i++] = "0123456789abcdef"[n % base];
        n /= base;
    } while (n);
    while (i < width--)
        terminal_putchar(pad);
    while (i)
        terminal_putchar(buf[--i]);
}

void printk(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    for (; *fmt; fmt++) {
        if (*fmt != '%') {
            terminal_putchar(*fmt);
            continue;
        }
        fmt++;
        char pad = ' ';
        int width = 0;
        if (*fmt == '0') {
            pad = '0';
            fmt++;
        }
        while (*fmt >= '0' && *fmt <= '9')
            width = width * 10 + (*fmt++ - '0');
        switch (*fmt) {
        case 'c':
            terminal_putchar((char)va_arg(ap, int));
            break;
        case 's': {
            const char *s = va_arg(ap, const char *);
            terminal_writestring(s ? s : "(null)");
            break;
        }
        case 'd': {
            int32_t v = va_arg(ap, int32_t);
            uint32_t u = (uint32_t)v;
            if (v < 0) {
                terminal_putchar('-');
                u = -u;
            }
            print_num(u, 10, width, pad);
            break;
        }
        case 'u':
            print_num(va_arg(ap, uint32_t), 10, width, pad);
            break;
        case 'x':
            print_num(va_arg(ap, uint32_t), 16, width, pad);
            break;
        case 'p':
            terminal_writestring("0x");
            print_num((uint32_t)va_arg(ap, void *), 16, 8, '0');
            break;
        case '%':
            terminal_putchar('%');
            break;
        case '\0':
            fmt--;
            break;
        default:
            terminal_putchar('%');
            terminal_putchar(*fmt);
            break;
        }
    }
    va_end(ap);
}
