#ifndef RC522_H
#define RC522_H

#include <stdint.h>

/*
 * NXP MFRC522 (the chip on the RC522 module) over lib/core/spi.h. Registers and
 * framing come from the MFRC522 datasheet (Components/MFRC522.pdf, docs/components/mfrc522.md),
 * not from the SVD.
 *
 *   RST  PB10  GPIO Arduino D6   -> NRSTPD (low = power-down, rising edge = reset)
 *   CS   PB6   the module's "SDA" pin is the chip select (NSS), from lib/core/spi.c
 *
 * SPI mode 0, MSB first, 10 Mbit/s maximum (datasheet 8.1.2). Every register
 * access is one address byte then data bytes with CS held low (8.1.2.1-3).
 * Polled only; every wait has a timeout and reports failure instead of hanging.
 */
#define RC522_REG_COMMAND       0x01u   /* Command[3:0] + RcvOff, PowerDown (9.3.1.2) */
#define RC522_REG_FIFO_DATA     0x09u   /* one byte in / out of the 64-byte FIFO (9.3.1.10) */
#define RC522_REG_FIFO_LEVEL    0x0Au   /* bit 7 FlushBuffer (write), bits 6..0 byte count (9.3.1.11) */
#define RC522_REG_AUTO_TEST     0x36u   /* SelfTest[3:0], reset value 0x40 (9.3.4.7) */
#define RC522_REG_VERSION       0x37u   /* read-only chip id: 0x91 = v1.0, 0x92 = v2.0 (9.3.4.8) */

#define RC522_FIFO_SIZE         64u
#define RC522_MEM_SIZE          25u     /* internal buffer, reachable only through the Mem command (10.3.1.2) */

/* Sets up the RST pin, resets the chip and waits for the oscillator. Call
   after spi_init() with mode 0. */
void rc522_init(void);

uint8_t rc522_read(uint8_t reg);
void rc522_write(uint8_t reg, uint8_t value);

/* Several bytes with one CS-low frame, for the FIFO. A burst read sends the
   address byte once per byte plus a final 0x00, and each reply is the byte
   asked for one step earlier (Table 6). A burst write sends the address once
   (Table 7). */
void rc522_read_burst(uint8_t reg, uint8_t *buf, uint32_t n);
void rc522_write_burst(uint8_t reg, const uint8_t *buf, uint32_t n);

/* SoftReset command, then waits until CommandReg reads idle again.
   Returns 0 on success, -1 on timeout. */
int rc522_soft_reset(void);

/*
 * Digital self test (datasheet 16.1.1): soft reset, clear the 25-byte internal
 * buffer, enable the test, run CalcCRC, collect the 64 bytes it leaves in the
 * FIFO. Compare them with the table for the chip version. Ends with a soft
 * reset, so the chip is back in its normal state.
 *
 * Returns 0 and fills out[64] on success; -1 if a soft reset timed out, -2 if
 * the Mem command did not finish, -3 if the FIFO never reached 64 bytes.
 */
int rc522_self_test(uint8_t out[RC522_FIFO_SIZE]);

/* Same, but skips the "clear the internal buffer" step, so the test runs on
   whatever the buffer holds. For finding out whether the buffer matters. */
int rc522_self_test_keep_buffer(uint8_t out[RC522_FIFO_SIZE]);

/*
 * The 25-byte internal buffer. It survives hard and soft reset (p.71, p.73).
 * Write: flush the FIFO, load 25 bytes, run Mem (FIFO -> buffer).
 * Read: flush the FIFO, run Mem on the empty FIFO (buffer -> FIFO), read it out.
 * Both return 0, -2 if Mem never finished, -3 (read only) if the FIFO did not
 * end up with 25 bytes.
 */
int rc522_mem_write(const uint8_t data[RC522_MEM_SIZE]);
int rc522_mem_read(uint8_t out[RC522_MEM_SIZE]);

/*
 * Talking to a tag (ISO/IEC 14443A over the RC522's antenna). The MCU is
 * always the SPI master: a tag's reply arrives by radio into the RC522's FIFO
 * and is read out over SPI like any register. See visualizations/spi/rc522-uid.html.
 *
 * Result codes (negative = no usable reply):
 */
#define RC522_OK            0
#define RC522_NO_TAG        (-1)    /* the timeout timer ran out: nobody answered */
#define RC522_ERR           (-2)    /* ErrorReg flagged the reply (parity, collision, ...) */
#define RC522_NO_FLAG       (-3)    /* the chip raised no flag in time (SPI or chip fault) */
#define RC522_BAD_FRAME     (-4)    /* reply has the wrong length or ends mid-byte */
#define RC522_BAD_BCC       (-5)    /* the 5th byte does not match the XOR of the UID bytes */
#define RC522_CASCADE       (-6)    /* UID starts with 0x88: a 7-byte UID, only 3 bytes are in uid[1..3] */

/* Timer (25 ms timeout), 100% ASK, antenna on. Call once after rc522_init(). */
void rc522_picc_init(void);

/*
 * One exchange: send tx_len bytes (the last one with tx_last_bits bits, 0 = all
 * 8), wait for the reply or the timeout, copy up to rx_max bytes to rx.
 * *rx_len and *rx_last_bits (valid bits in the last byte, 0 = all) are set on
 * RC522_OK. Needs the antenna on (picc_init).
 */
int rc522_transceive(const uint8_t *tx, uint32_t tx_len, uint32_t tx_last_bits,
                     uint8_t *rx, uint32_t rx_max, uint32_t *rx_len, uint32_t *rx_last_bits);

/* REQA (26h) or, with wake != 0, WUPA (52h): a 7-bit frame. The tag answers
   with 2 bytes, the ATQA. 00 04h = 4-byte UID, 00 44h = 7-byte UID (MF1S50 p.15). */
int rc522_request(uint8_t atqa[2], int wake);

/* Anticollision, cascade level 1 (93h 20h): the tag answers with 4 UID bytes
   and a check byte. RC522_OK when the check byte matches. */
int rc522_read_uid(uint8_t uid[4]);

#endif
