/* kprintf.c - kernel printf for debugging output */
#include "kprintf.h"
#include "vga.h"
#include "serial.h"
#include <stdarg.h>
#include <stdint.h>

void putc(char c) {
    vga_putch(c);
    serial_putc(c);
}

static void prints(const char *s) {
    if (!s) {
        /* print "(null)" for NULL strings */
        const char *nullmsg = "(null)";
        while (*nullmsg) putc(*nullmsg++);
        return;
    }
    while (*s) putc(*s++);
}

/* print unsigned number in given base (base between 2 and 16) */
static void print_unsigned(unsigned long n, int base, int uppercase) {
    static const char hexdigits_l[] = "0123456789abcdef";
    static const char hexdigits_u[] = "0123456789ABCDEF";
    const char *hexdigits = uppercase ? hexdigits_u : hexdigits_l;
    char buf[32];
    int i = 0;

    if (n == 0) {
        putc('0');
        return;
    }

    while (n > 0 && i < (int)sizeof(buf)) {
        buf[i++] = hexdigits[n % base];
        n /= base;
    }

    while (i-- > 0) putc(buf[i]);
}

/* print signed long in base 10 */
static void print_signed(long n) {
    if (n < 0) {
        putc('-');
        /* convert to unsigned to avoid UB for INT_MIN */
        print_unsigned((unsigned long)(-(n + 1)) + 1, 10, 0);
    } else {
        print_unsigned((unsigned long)n, 10, 0);
    }
}

void kprintf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);

    while (*fmt) {
        if (*fmt == '%') {
            ++fmt;
            int longmod = 0;

            /* support length modifier 'l' (e.g., %ld, %lu, %lx, %lp) */
            if (*fmt == 'l') {
                longmod = 1;
                ++fmt;
            }

            switch (*fmt) {
                case 'd':
                case 'i':
                    if (longmod) {
                        print_signed(va_arg(ap, long));
                    } else {
                        print_signed((long)va_arg(ap, int));
                    }
                    break;
                case 'u':
                    if (longmod) {
                        print_unsigned(va_arg(ap, unsigned long), 10, 0);
                    } else {
                        print_unsigned((unsigned long)va_arg(ap, unsigned int), 10, 0);
                    }
                    break;
                case 'x':
                    if (longmod) {
                        print_unsigned(va_arg(ap, unsigned long), 16, 0);
                    } else {
                        print_unsigned((unsigned long)va_arg(ap, unsigned int), 16, 0);
                    }
                    break;
                case 'X':
                    if (longmod) {
                        print_unsigned(va_arg(ap, unsigned long), 16, 1);
                    } else {
                        print_unsigned((unsigned long)va_arg(ap, unsigned int), 16, 1);
                    }
                    break;
                case 'p':
                    prints("0x");
                    /* pointer promoted to void*, print as unsigned long */
                    print_unsigned((unsigned long)va_arg(ap, void *), 16, 0);
                    break;
                case 's':
                    prints(va_arg(ap, const char *));
                    break;
                case 'c':
                    putc((char)va_arg(ap, int));
                    break;
                case '%':
                    putc('%');
                    break;
                default:
                    /* unknown specifier: print it literally */
                    putc('%');
                    if (longmod) putc('l');
                    if (*fmt) putc(*fmt);
                    break;
            }
            ++fmt;
        } else if (*fmt == '\n') {
            putc('\r');
            putc('\n');
            ++fmt;
        } else if (*fmt == '\t') {
            /* expand tab to 4 spaces */
            putc(' ');
            putc(' ');
            putc(' ');
            putc(' ');
            ++fmt;
        } else {
            putc(*fmt++);
        }
    }

    va_end(ap);
}