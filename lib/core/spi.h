#ifndef SPI_H
#define SPI_H

#include <stdint.h>

/*
 * SPI1 as a polled master, with a software chip select.
 *
 *   SCK  PB3  AF5  Arduino D3
 *   MISO PB4  AF5  Arduino D5
 *   MOSI PB5  AF5  Arduino D4
 *   CS   PB6  GPIO Arduino D10   (idle high, driven by spi_cs_low/high)
 *
 * SCK = PCLK2 / 2^(br+1), so br 0 is 45 MHz at 180 MHz sysclk (the SPI1 master
 * maximum, datasheet table 63) and br 7 is 351.5 kHz. Call clock_init() first
 * if the program uses it: the rate is computed from the bus clock at the time
 * of the call, and spi_sck_hz() reports it.
 *
 * Polled only: no interrupts and no DMA. One owner, one thread.
 */
#define SPI_BR_FASTEST  0u
#define SPI_BR_SLOWEST  7u

/* mode bit 1 = CPOL, bit 0 = CPHA (mode 0 = idle low, sample on rising edge). */
void spi_init(uint32_t br, uint32_t mode);

/* Changes rate and mode. Waits for a running transfer, because BR, CPOL and
   CPHA must not change while one is in progress (RM0390 26.7.1). */
void spi_configure(uint32_t br, uint32_t mode);
uint32_t spi_sck_hz(uint32_t br);

/* One byte each way: SCK runs 8 pulses, the byte shifted in is returned. */
uint8_t spi_xfer(uint8_t out);

void spi_cs_low(void);
void spi_cs_high(void);          /* waits for the last bit to leave first */

#endif
