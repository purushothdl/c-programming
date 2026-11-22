#include "stm32f4xx.h"
#include "delay.h"
#include "rc522.h"
#include "spi.h"

/* STM32 pin PB10, which is Arduino D6 (wired to the module's RST). Not D10:
   Arduino D10 is PB6, the chip select in lib/core/spi.c (the module's "SDA"). */
#define RST_PIN         10u
#define ADDR_READ       0x80u               /* bit 7: 1 = read, 0 = write (datasheet 8.1.2.3) */

/* CommandReg command codes (Table 149, p.70). */
#define CMD_IDLE        0x00u
#define CMD_MEM         0x01u
#define CMD_CALC_CRC    0x03u
#define CMD_SOFT_RESET  0x0Fu
#define CMD_MASK        0x0Fu               /* Command[3:0] */
#define CMD_POWER_DOWN  0x10u               /* PowerDown bit, 9.3.1.2 */

#define FIFO_FLUSH      0x80u               /* FIFOLevelReg.FlushBuffer */
#define FIFO_LEVEL_MASK 0x7Fu

#define AUTOTEST_ON     0x09u               /* SelfTest[3:0] = 1001b (9.3.4.7) */
#define AUTOTEST_RESET  0x40u               /* reset value: self test off, AmpRcv on */

/* Each poll is a 2-byte SPI read (46 us at 351 kHz). Generous bounds, so a
   dead chip is reported in well under a second. */
#define POLL_LIMIT      2000u

/* Reset low must last >= 100 ns (8.8.1). After the rising edge the oscillator
   needs 37.74 us plus the crystal's start-up, which the datasheet does not
   bound (8.8.2): 50 ms is a generous guess, not a measured value. */
#define RESET_LOW_US        10u
#define RESET_START_MS      50u

void rc522_init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;

    /* Low = power-down, rising edge = reset (7.1). Low must last >= 100 ns (8.8.1). */
    GPIOB->BSRR = GPIO_BSRR_BR10;
    GPIOB->MODER &= ~GPIO_MODER_MODER10;
    GPIOB->MODER |= GPIO_MODER_MODER10_0;
    delay_us(RESET_LOW_US);

    GPIOB->BSRR = GPIO_BSRR_BS10;
    delay_ms(RESET_START_MS);
}

/* Reply to byte 1 is don't-care; byte 2 carries the register (Table 6). */
uint8_t rc522_read(uint8_t reg)
{
    spi_cs_low();
    (void)spi_xfer((uint8_t)(ADDR_READ | (reg << 1)));
    uint8_t value = spi_xfer(0x00u);
    spi_cs_high();
    return value;
}

void rc522_write(uint8_t reg, uint8_t value)
{
    spi_cs_low();
    (void)spi_xfer((uint8_t)(reg << 1));
    (void)spi_xfer(value);
    spi_cs_high();
}

void rc522_read_burst(uint8_t reg, uint8_t *buf, uint32_t n)
{
    spi_cs_low();
    for (uint32_t i = 0u; i <= n; i++) {
        /* Reply to byte i arrives one transfer late; the last 0x00 collects it. */
        uint8_t reply = spi_xfer((i < n) ? (uint8_t)(ADDR_READ | (reg << 1)) : 0x00u);
        if (i > 0u) {
            buf[i - 1u] = reply;
        }
    }
    spi_cs_high();
}

void rc522_write_burst(uint8_t reg, const uint8_t *buf, uint32_t n)
{
    spi_cs_low();
    (void)spi_xfer((uint8_t)(reg << 1));
    for (uint32_t i = 0u; i < n; i++) {
        (void)spi_xfer(buf[i]);
    }
    spi_cs_high();
}

/* A command that ends by itself leaves Command[3:0] = Idle. */
static int wait_idle(void)
{
    for (uint32_t i = 0u; i < POLL_LIMIT; i++) {
        if ((rc522_read(RC522_REG_COMMAND) & (CMD_MASK | CMD_POWER_DOWN)) == 0u) {
            return 0;
        }
    }
    return -1;
}

int rc522_soft_reset(void)
{
    rc522_write(RC522_REG_COMMAND, CMD_SOFT_RESET);
    delay_ms(1u);                       /* let the reset begin before polling */
    return wait_idle();
}

int rc522_mem_write(const uint8_t data[RC522_MEM_SIZE])
{
    rc522_write(RC522_REG_FIFO_LEVEL, FIFO_FLUSH);
    rc522_write_burst(RC522_REG_FIFO_DATA, data, RC522_MEM_SIZE);
    rc522_write(RC522_REG_COMMAND, CMD_MEM);
    return wait_idle() == 0 ? 0 : -2;
}

int rc522_mem_read(uint8_t out[RC522_MEM_SIZE])
{
    /* Mem on an empty FIFO copies the internal buffer out into the FIFO
       (10.3.1.2, p.71). */
    rc522_write(RC522_REG_FIFO_LEVEL, FIFO_FLUSH);
    rc522_write(RC522_REG_COMMAND, CMD_MEM);
    if (wait_idle() != 0) {
        return -2;
    }
    if ((rc522_read(RC522_REG_FIFO_LEVEL) & FIFO_LEVEL_MASK) != RC522_MEM_SIZE) {
        return -3;
    }
    rc522_read_burst(RC522_REG_FIFO_DATA, out, RC522_MEM_SIZE);
    return 0;
}

static int self_test(uint8_t out[RC522_FIFO_SIZE], int clear_buffer)
{
    static const uint8_t zeros[RC522_MEM_SIZE];     /* all 0x00 */
    int rc = 0;

    if (rc522_soft_reset() != 0) {
        return -1;
    }

    /* Datasheet step 2 says "Config command"; it is not in Table 149. Mem is
       the command that fills the 25-byte buffer (10.3.1.2). */
    if (clear_buffer && rc522_mem_write(zeros) != 0) {
        return -2;
    }

    rc522_write(RC522_REG_AUTO_TEST, AUTOTEST_ON);
    /* The flush is mine, not in the procedure (p.82); the datasheet does not
       say why the 0x00 is needed. */
    rc522_write(RC522_REG_FIFO_LEVEL, FIFO_FLUSH);
    rc522_write(RC522_REG_FIFO_DATA, 0x00u);
    rc522_write(RC522_REG_COMMAND, CMD_CALC_CRC);

    /* No end flag is documented: wait for the 64-byte result in the FIFO. */
    uint32_t level = 0u;
    for (uint32_t i = 0u; i < POLL_LIMIT && level < RC522_FIFO_SIZE; i++) {
        level = rc522_read(RC522_REG_FIFO_LEVEL) & FIFO_LEVEL_MASK;
    }
    if (level < RC522_FIFO_SIZE) {
        rc = -3;
    }

    /* CalcCRC does not end by itself (10.3.1.4): stop it before reading. */
    rc522_write(RC522_REG_COMMAND, CMD_IDLE);
    if (rc == 0) {
        rc522_read_burst(RC522_REG_FIFO_DATA, out, RC522_FIFO_SIZE);
    }

    /* Test off for normal use (9.3.4.7); the reset also restores the defaults. */
    rc522_write(RC522_REG_AUTO_TEST, AUTOTEST_RESET);
    (void)rc522_soft_reset();
    return rc;
}

int rc522_self_test(uint8_t out[RC522_FIFO_SIZE])
{
    return self_test(out, 1);
}

int rc522_self_test_keep_buffer(uint8_t out[RC522_FIFO_SIZE])
{
    return self_test(out, 0);
}

/* ---- talking to a tag ---- */

#define REG_COMIRQ      0x04u
#define REG_ERROR       0x06u
#define REG_CONTROL     0x0Cu
#define REG_BITFRAMING  0x0Du
#define REG_COLL        0x0Eu
#define REG_TXCONTROL   0x14u
#define REG_TXASK       0x15u
#define REG_TMODE       0x2Au
#define REG_TPRESCALER  0x2Bu
#define REG_TRELOAD_H   0x2Cu
#define REG_TRELOAD_L   0x2Du

#define CMD_TRANSCEIVE  0x0Cu

/* ComIrqReg bits (p.39). Writing with bit 7 clear clears the 1 bits (Set1, p.40). */
#define IRQ_CLEAR_ALL   0x7Fu
#define IRQ_RX          0x20u
#define IRQ_IDLE        0x10u
#define IRQ_ERR         0x02u
#define IRQ_TIMER       0x01u

#define BITFRAMING_START_SEND   0x80u
#define ERR_MASK        0xDFu               /* every ErrorReg bit except the reserved bit 5 (p.41) */

/* tick = 13.56 MHz / (2 x 169 + 1) = 25 us; timeout = 1001 ticks = about 25 ms (p.32, p.61). */
#define TPRESCALER_VAL  169u
#define TRELOAD_VAL     1000u
#define TMODE_TAUTO     0x80u               /* timer starts by itself when a send ends (p.61) */
#define TXASK_100ASK    0x40u               /* Force100ASK (p.51) */
#define TXCONTROL_ANT   0x03u               /* Tx2RFEn | Tx1RFEn: antenna drivers on (p.50) */

#define CMD_REQA        0x26u
#define CMD_WUPA        0x52u
#define CMD_SEL_CL1     0x93u
#define CMD_NVB_2BYTES  0x20u               /* "2 valid bytes so far": ISO/IEC 14443-3, not in MF1S50 */
#define CASCADE_TAG     0x88u               /* ISO/IEC 14443-3: first byte of a longer UID */

void rc522_picc_init(void)
{
    rc522_write(REG_TMODE, TMODE_TAUTO);
    rc522_write(REG_TPRESCALER, TPRESCALER_VAL);
    rc522_write(REG_TRELOAD_H, (uint8_t)(TRELOAD_VAL >> 8));
    rc522_write(REG_TRELOAD_L, (uint8_t)(TRELOAD_VAL & 0xFFu));
    rc522_write(REG_TXASK, TXASK_100ASK);
    rc522_write(REG_COLL, 0x00u);           /* ValuesAfterColl = 0: reset value is undefined (p.46) */

    /* Antenna is off at reset: no field, no power for a tag. TxWaitRF (p.48)
       blocks sending without a field. */
    rc522_write(REG_TXCONTROL, (uint8_t)(rc522_read(REG_TXCONTROL) | TXCONTROL_ANT));
}

int rc522_transceive(const uint8_t *tx, uint32_t tx_len, uint32_t tx_last_bits,
                     uint8_t *rx, uint32_t rx_max, uint32_t *rx_len, uint32_t *rx_last_bits)
{
    /* ComIrqReg resets to 0x14: clear stale flags first. */
    rc522_write(REG_COMIRQ, IRQ_CLEAR_ALL);
    rc522_write(RC522_REG_FIFO_LEVEL, FIFO_FLUSH);
    rc522_write_burst(RC522_REG_FIFO_DATA, tx, tx_len);     /* data first: commands do not clear the FIFO (p.70) */
    rc522_write(REG_BITFRAMING, (uint8_t)tx_last_bits);

    /* Plain 0x0C: CommandReg resets with RcvOff = 1 (p.38). */
    rc522_write(RC522_REG_COMMAND, CMD_TRANSCEIVE);
    rc522_write(REG_BITFRAMING, (uint8_t)(BITFRAMING_START_SEND | tx_last_bits));

    uint8_t irq = 0u;
    for (uint32_t i = 0u; i < POLL_LIMIT; i++) {
        irq = rc522_read(REG_COMIRQ);
        if ((irq & (IRQ_RX | IRQ_IDLE | IRQ_ERR | IRQ_TIMER)) != 0u) {
            break;
        }
    }

    /* ErrorReg first: the next command clears it (p.41). Transceive needs an Idle (p.72). */
    uint8_t err = rc522_read(REG_ERROR);
    rc522_write(RC522_REG_COMMAND, CMD_IDLE);

    if ((irq & IRQ_RX) == 0u) {
        if ((irq & IRQ_TIMER) != 0u) {
            return RC522_NO_TAG;
        }
        return (irq & IRQ_ERR) != 0u ? RC522_ERR : RC522_NO_FLAG;
    }
    if ((err & ERR_MASK) != 0u) {
        return RC522_ERR;
    }

    uint32_t n = rc522_read(RC522_REG_FIFO_LEVEL) & FIFO_LEVEL_MASK;
    if (n == 0u || n > rx_max) {
        return RC522_BAD_FRAME;
    }
    rc522_read_burst(RC522_REG_FIFO_DATA, rx, n);
    *rx_len = n;
    *rx_last_bits = rc522_read(REG_CONTROL) & 0x07u;
    return RC522_OK;
}

int rc522_request(uint8_t atqa[2], int wake)
{
    uint8_t cmd = (wake != 0) ? CMD_WUPA : CMD_REQA;
    uint32_t n;
    uint32_t last;

    /* REQA is a 7-bit frame: TxLastBits = 7 (MF1S50 p.13, BitFramingReg p.46). */
    int rc = rc522_transceive(&cmd, 1u, 7u, atqa, 2u, &n, &last);
    if (rc != RC522_OK) {
        return rc;
    }
    return (n == 2u && last == 0u) ? RC522_OK : RC522_BAD_FRAME;
}

int rc522_read_uid(uint8_t uid[4])
{
    static const uint8_t cmd[2] = { CMD_SEL_CL1, CMD_NVB_2BYTES };   /* 93h 20h, MF1S50 p.13 */
    uint8_t rx[5];
    uint32_t n;
    uint32_t last;

    int rc = rc522_transceive(cmd, 2u, 0u, rx, sizeof rx, &n, &last);
    if (rc != RC522_OK) {
        return rc;
    }
    if (n != 5u || last != 0u) {
        return RC522_BAD_FRAME;
    }

    /* Byte 5 = XOR of bytes 1-4 (ISO 14443-3): catches damage, not forgery. */
    uint8_t bcc = (uint8_t)(rx[0] ^ rx[1] ^ rx[2] ^ rx[3]);
    for (uint32_t i = 0u; i < 4u; i++) {
        uid[i] = rx[i];
    }
    if (bcc != rx[4]) {
        return RC522_BAD_BCC;
    }
    return (rx[0] == CASCADE_TAG) ? RC522_CASCADE : RC522_OK;
}
