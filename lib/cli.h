#ifndef CLI_H
#define CLI_H

#include <stdint.h>

/*
 * Tiny command line over the UART (lib/uart.h must be initialised first).
 *
 * Built in:
 *   help
 *   peek <addr>            read a 32-bit word
 *   poke <addr> <val>      write a 32-bit word
 *
 * Each program adds its own commands by passing a table to cli_start().
 * Numbers may be decimal or 0x-prefixed hex. Addresses must be 4-byte aligned.
 */
typedef struct {
    const char *name;                  /* first word of the line */
    uint8_t nargs;                     /* words expected after the name, at most 3 */
    const char *usage;                 /* shown by help, e.g. "led on|off" */
    void (*run)(char *args[]);         /* args[0..nargs-1], valid only during the call */
} cli_cmd_t;

/* The table must outlive the CLI: pass a static array. */
void cli_start(const cli_cmd_t *cmds, uint32_t count);   /* banner and first prompt */
void cli_feed(char c);              /* one received character: echo, edit, or run on Enter */

#endif
