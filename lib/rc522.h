#ifndef RC522_H
#define RC522_H

#include <stdint.h>

/*
 * NXP MFRC522 (the chip on the RC522 module) over lib/spi.h. Registers and
 * framing come from the MFRC522 datasheet (Components/MFRC522.pdf, docs/mfrc522.md),
 * not from the SVD.
 *
 *   RST  PB10  GPIO Arduino D6   -> NRSTPD (low = power-down, rising edge = reset)
 *   CS   PB6   the module's "SDA" pin is the chip select (NSS), from lib/spi.c
 *
 * SPI mode 0, MSB first, 10 Mbit/s maximum (datasheet 8.1.2). Every register
 * access is one address byte then one data byte with CS held low (8.1.2.1-3).
 */
#define RC522_REG_VERSION   0x37u   /* read-only chip id: 0x91 = v1.0, 0x92 = v2.0 (9.3.4.8) */

/* Sets up the RST pin, resets the chip and waits for the oscillator. Call
   after spi_init() with mode 0. */
void rc522_init(void);

uint8_t rc522_read(uint8_t reg);
void rc522_write(uint8_t reg, uint8_t value);

#endif
