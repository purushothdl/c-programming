#include "stm32f4xx.h"
#include "clock.h"
#include "rc522.h"
#include "spi.h"

/* STM32 pin PB10, which is Arduino D6 (wired to the module's RST). Not D10:
   Arduino D10 is PB6, the chip select in lib/spi.c (the module's "SDA"). */
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

/* Roughly 5 ms. The datasheet gives the internal start-up delay as 37.74 us
   plus the crystal's own start-up time (8.8.2), which it does not bound. A
   loop iteration takes at least one cycle, so this waits at least that long. */
#define RESET_WAIT_CYCLES   (clock_sysclk_hz() / 200u)

static void wait_cycles(uint32_t n)
{
    for (volatile uint32_t i = 0u; i < n; i++) {
    }
}

void rc522_init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;

    /* Drive low first (power-down), then high: the rising edge is the reset
       (datasheet 7.1). Low must last 100 ns or more (8.8.1); the wait below
       covers it. */
    GPIOB->BSRR = GPIO_BSRR_BR10;
    GPIOB->MODER &= ~GPIO_MODER_MODER10;
    GPIOB->MODER |= GPIO_MODER_MODER10_0;
    wait_cycles(RESET_WAIT_CYCLES);

    GPIOB->BSRR = GPIO_BSRR_BS10;
    wait_cycles(RESET_WAIT_CYCLES);
}

/* The chip decodes the address byte while it is still being shifted in, so
   what comes back during byte 1 is "don't care" and byte 2 carries the
   register (Table 6). */
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
        /* Ask for byte i with the address byte; its reply arrives while the
           next byte is sent. The last round sends 0x00 only to collect it. */
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
    wait_cycles(RESET_WAIT_CYCLES);
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

    /* Step 2 says "Config command", which is not in the command list (Table
       149). Mem is the command that moves 25 bytes from the FIFO into the
       internal buffer (10.3.1.2), so that is what runs here. */
    if (clear_buffer && rc522_mem_write(zeros) != 0) {
        return -2;
    }

    rc522_write(RC522_REG_AUTO_TEST, AUTOTEST_ON);
    /* The flush is not in the datasheet procedure (p.82). It makes the 0x00
       below the only byte in the FIFO, so the run matches the procedure's.
       The datasheet does not say why that 0x00 is needed. */
    rc522_write(RC522_REG_FIFO_LEVEL, FIFO_FLUSH);
    rc522_write(RC522_REG_FIFO_DATA, 0x00u);
    rc522_write(RC522_REG_COMMAND, CMD_CALC_CRC);

    /* The datasheet does not say how to detect the end of the test; its
       result is 64 bytes in the FIFO, so wait for the level to reach 64. */
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

    /* Normal operation needs the test switched off (9.3.4.7); the soft reset
       also leaves every other register at its default. */
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
