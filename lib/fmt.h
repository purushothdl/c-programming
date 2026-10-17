#ifndef FMT_H
#define FMT_H

#include <stdint.h>

/*
 * Printing, parsing and string helpers. Pure logic, no hardware.
 *
 * The print helpers take the character sink as a parameter, so the same code
 * path prints through the interrupt-driven queue normally and through the
 * polled writer inside HardFault.
 */
typedef void (*putc_fn)(char);

void print_str(putc_fn put, const char *s);
void print_hex32(putc_fn put, uint32_t v);
void print_dec32(putc_fn put, uint32_t v);

/* Accepts 0x-prefixed hex or plain decimal. Returns 0 on empty input, bad
   digits, or a value that does not fit in 32 bits; *out is only written on 1. */
int parse_u32(const char *s, uint32_t *out);

int streq(const char *a, const char *b);

/* Splits s in place into up to max words; returns the word count. */
int tokenize(char *s, char *argv[], int max);

#endif
