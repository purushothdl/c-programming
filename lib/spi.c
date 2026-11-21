#include "stm32f4xx.h"
#include "clock.h"
#include "spi.h"

#define SPI1_AF         5u
#define SCK_PIN         3u
#define MISO_PIN        4u
#define MOSI_PIN        5u

static void pin_af5(uint32_t pin)
{
    GPIOB->MODER &= ~(3u << (pin * 2u));
    GPIOB->MODER |= 2u << (pin * 2u);
    GPIOB->AFR[0] &= ~(0xFu << (pin * 4u));
    GPIOB->AFR[0] |= SPI1_AF << (pin * 4u);
    GPIOB->PUPDR &= ~(3u << (pin * 2u));    /* PB4 has a pull-up after reset (NJTRST): remove it */
    GPIOB->OSPEEDR &= ~(3u << (pin * 2u));
    GPIOB->OSPEEDR |= 2u << (pin * 2u);     /* "fast": the speed table 63 is specified at */
}

static void pins_init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;

    pin_af5(SCK_PIN);
    pin_af5(MISO_PIN);
    pin_af5(MOSI_PIN);

    /* CS is idle high. Set the output level before switching to output mode,
       so the pin never drives low for a moment. */
    GPIOB->BSRR = GPIO_BSRR_BS6;
    GPIOB->MODER &= ~GPIO_MODER_MODER6;
    GPIOB->MODER |= GPIO_MODER_MODER6_0;
}

void spi_configure(uint32_t br, uint32_t mode)
{
    while ((SPI1->SR & SPI_SR_BSY) != 0u) {
    }
    SPI1->CR1 &= ~SPI_CR1_SPE;

    /* MSTR: this side drives SCK.
       SSM + SSI: NSS is managed in software and held "high" internally. With
       SSI = 0 the master would think another master selected it and fault
       (MODF clears MSTR and SPE, RM0390 26.3.13). The real chip select is the
       PB6 GPIO, driven by hand. */
    SPI1->CR1 = SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI |
                (br << SPI_CR1_BR_Pos) |
                (((mode & 2u) != 0u) ? SPI_CR1_CPOL : 0u) |
                (((mode & 1u) != 0u) ? SPI_CR1_CPHA : 0u);
    SPI1->CR1 |= SPI_CR1_SPE;
}

void spi_init(uint32_t br, uint32_t mode)
{
    pins_init();
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;
    SPI1->CR2 = 0u;                         /* no interrupts, no DMA, Motorola frame format */
    spi_configure(br, mode);
}

uint32_t spi_sck_hz(uint32_t br)
{
    return clock_pclk2_hz() >> (br + 1u);
}

/* Writing DR starts 8 SCK pulses; the byte shifted in at the same time lands
   in DR when RXNE sets. Reading DR every time keeps RXNE from overflowing
   into OVR. */
uint8_t spi_xfer(uint8_t out)
{
    while ((SPI1->SR & SPI_SR_TXE) == 0u) {
    }
    SPI1->DR = out;
    while ((SPI1->SR & SPI_SR_RXNE) == 0u) {
    }
    return (uint8_t)SPI1->DR;
}

/* TXE first, then BSY (RM0390 26.3.10; BSY sets 2 APB clocks after the DR write). */
static void wait_idle(void)
{
    while ((SPI1->SR & SPI_SR_TXE) == 0u) {
    }
    while ((SPI1->SR & SPI_SR_BSY) != 0u) {
    }
}

void spi_cs_low(void)
{
    GPIOB->BSRR = GPIO_BSRR_BR6;
}

void spi_cs_high(void)
{
    wait_idle();
    GPIOB->BSRR = GPIO_BSRR_BS6;
}
