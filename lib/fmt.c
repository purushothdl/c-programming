#include "fmt.h"

void print_str(putc_fn put, const char *s)
{
    while (*s != '\0') {
        put(*s++);
    }
}

void print_hex32(putc_fn put, uint32_t v)
{
    static const char digits[] = "0123456789ABCDEF";

    put('0');
    put('x');
    for (int shift = 28; shift >= 0; shift -= 4) {
        put(digits[(v >> shift) & 0xFu]);
    }
}

void print_dec32(putc_fn put, uint32_t v)
{
    char buf[10];
    uint32_t n = 0u;

    do {
        buf[n++] = (char)('0' + (v % 10u));
        v /= 10u;
    } while (v != 0u);

    while (n != 0u) {
        put(buf[--n]);
    }
}

static int hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

int parse_u32(const char *s, uint32_t *out)
{
    uint32_t base = 10u;
    uint32_t v = 0u;

    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        base = 16u;
        s += 2;
    }
    if (*s == '\0') {
        return 0;
    }

    for (; *s != '\0'; s++) {
        int d = hexval(*s);
        if (d < 0 || (uint32_t)d >= base) {
            return 0;
        }
        if (v > (0xFFFFFFFFu - (uint32_t)d) / base) {
            return 0;                       /* would overflow */
        }
        v = v * base + (uint32_t)d;
    }
    *out = v;
    return 1;
}

int streq(const char *a, const char *b)
{
    while (*a != '\0' && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

int tokenize(char *s, char *argv[], int max)
{
    int n = 0;

    while (*s != '\0' && n < max) {
        while (*s == ' ') {
            s++;
        }
        if (*s == '\0') {
            break;
        }
        argv[n++] = s;
        while (*s != '\0' && *s != ' ') {
            s++;
        }
        if (*s != '\0') {
            *s++ = '\0';
        }
    }
    return n;
}
