#include "stm32f4xx.h"
#include "clock.h"
#include "rc522.h"
#include "spi.h"

#define RST_PIN         10u
#define ADDR_READ       0x80u               /* bit 7: 1 = read, 0 = write (datasheet 8.1.2.3) */

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
