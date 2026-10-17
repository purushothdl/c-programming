#ifndef CLI_H
#define CLI_H

/*
 * Tiny command line over the UART.
 *
 *   help
 *   led on | off
 *   ticks                  milliseconds since reset (SysTick)
 *   peek <addr>            read a 32-bit word
 *   poke <addr> <val>      write a 32-bit word
 *
 * Numbers may be decimal or 0x-prefixed hex. Addresses must be 4-byte aligned.
 */
void cli_start(void);               /* banner and first prompt */
void cli_feed(char c);              /* one received character: echo, edit, or run on Enter */

#endif
