#include <stdarg.h>
#include "stdint.h"
#include "stddef.h"
#include "printk.h"
#include "terminal.h"

/* Printing a string, return number of chars printed */
static int print_str(const char *s)
{
    int n = 0;
    if (!s)
        s = "(null)";
    while (*s) { terminal_putchar(*s++); n++; }
    return n;
}

/* printing numbers in base (10 or 16), left-padded with `pad` up to `width` */
static int print_uint(uint32_t value, uint32_t base, int upper, int width, char pad)
{
    char buf[32];
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    int i = 0;

    do {
        buf[i++] = digits[value % base];   /* taking the last digit */
        value /= base;
    } while (value);

    int n = i;
    for (; n < width; n++)
        terminal_putchar(pad);
    while (i--)                            /* printing REVERSE */
        terminal_putchar(buf[i]);
    return n;
}

/* signed numbers (base 10) */
static int print_int(int32_t value, int width, char pad)
{
    if (value < 0) {
        terminal_putchar('-');
        /* unsigned negation: also correct for INT32_MIN */
        return 1 + print_uint(0u - (uint32_t)value, 10, 0, width - 1, pad);
    }
    return print_uint((uint32_t)value, 10, 0, width, pad);
}

int printk(const char *fmt, ...)
{
    va_list ap;
    int count = 0;

    va_start(ap, fmt);
    for (size_t i = 0; fmt[i]; i++) {
        if (fmt[i] != '%') {                       /* normal char */
            terminal_putchar(fmt[i]);
            count++;
            continue;
        }
        i++;                                       /* skip '%', parse the rest */

        char pad = ' ';                            /* optional "0" flag + width */
        int width = 0;
        if (fmt[i] == '0') {
            pad = '0';
            i++;
        }
        while (fmt[i] >= '0' && fmt[i] <= '9')
            width = width * 10 + (fmt[i++] - '0');

        switch (fmt[i]) {
            case 'c': terminal_putchar((char)va_arg(ap, int)); count++; break;
            case 's': count += print_str(va_arg(ap, const char *)); break;
            case 'd':
            case 'i': count += print_int(va_arg(ap, int32_t), width, pad); break;
            case 'u': count += print_uint(va_arg(ap, uint32_t), 10, 0, width, pad); break;
            case 'x': count += print_uint(va_arg(ap, uint32_t), 16, 0, width, pad); break;
            case 'X': count += print_uint(va_arg(ap, uint32_t), 16, 1, width, pad); break;
            case 'p': count += print_str("0x");    /* always 8 digits: 0x00207fa0 */
                      count += print_uint((uint32_t)va_arg(ap, void *), 16, 0, 8, '0'); break;
            case '%': terminal_putchar('%'); count++; break;
            case '\0': i--; break;                 /* lone '%' at the end */
            default:  terminal_putchar('%'); terminal_putchar(fmt[i]); count += 2; break;
        }
    }
    va_end(ap);
    return count;
}
