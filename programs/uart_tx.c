#include "stm32f4xx.h"

/*
 * Step 5a: polling USART2 transmit.
 *
 * PA2 is USART2_TX and PA3 is USART2_RX on AF7. On the NUCLEO-F446RE,
 * USART2 is connected to the ST-LINK virtual COM port by default.
 *
 * The MCU is still using the reset-default 16 MHz HSI clock. The 1 ms SysTick
 * interrupt provides the schedule; USART2 itself is polled for each byte.
 */
#define CPU_CLK_HZ 16000000u
#define USART2_CLK_HZ 16000000u /* PCLK1 at the reset-default clock setup */
#define SYSTICK_HZ 1000u
#define UART_BAUD 115200u
#define UART_BRR ((USART2_CLK_HZ + (UART_BAUD / 2u)) / UART_BAUD)

#define UART_TX_PIN 2u
#define UART_RX_PIN 3u
#define UART_AF 7u

static volatile uint32_t g_ms_ticks;

/* SysTick exception (15): increment the millisecond time base. */
void SysTick_Handler(void)
{
    g_ms_ticks++;
}

static void systick_init(void)
{
    SysTick->LOAD = (CPU_CLK_HZ / SYSTICK_HZ) - 1u; /* 16000 - 1 */
    SysTick->VAL = 0u;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk |
                    SysTick_CTRL_TICKINT_Msk   |
                    SysTick_CTRL_ENABLE_Msk;
}

static void uart_init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;       /* GPIOA clock */
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;      /* USART2 clock (PCLK1) */

    /* PA2 and PA3: alternate-function mode (10). */
    GPIOA->MODER &= ~(GPIO_MODER_MODER2 | GPIO_MODER_MODER3);
    GPIOA->MODER |= GPIO_MODER_MODER2_1 | GPIO_MODER_MODER3_1;

    /* AF7 selects USART2_TX on PA2 and USART2_RX on PA3. */
    GPIOA->AFR[0] &= ~(GPIO_AFRL_AFSEL2 | GPIO_AFRL_AFSEL3);
    GPIOA->AFR[0] |= (UART_AF << GPIO_AFRL_AFSEL2_Pos) |
                     (UART_AF << GPIO_AFRL_AFSEL3_Pos);

    /* UART TX is push-pull; use no GPIO pull on either AF pin. */
    GPIOA->OTYPER &= ~(GPIO_OTYPER_OT2 | GPIO_OTYPER_OT3);
    GPIOA->PUPDR &= ~(GPIO_PUPDR_PUPDR2 | GPIO_PUPDR_PUPDR3);

    /* Configure USART while disabled: 8 data bits, no parity, 1 stop bit. */
    USART2->CR1 = 0u;
    USART2->CR2 = 0u;
    USART2->CR3 = 0u;

    /* OVER8=0: USARTDIV = PCLK1/(16*baud) = 8.6806.
       Rounded BRR = 16*USARTDIV = PCLK1/baud = 139 = 0x008B.
       In BRR, that is mantissa 8 and fraction 11/16. */
    USART2->BRR = UART_BRR;

    /* Enable transmitter and USART; receiver remains disabled for this step. */
    USART2->CR1 = USART_CR1_TE | USART_CR1_UE;
}

static void uart_write_char(char ch)
{
    while ((USART2->SR & USART_SR_TXE) == 0u) {
        /* Wait until the transmit data register can accept another byte. */
    }

    USART2->DR = (uint8_t)ch;
}

static void uart_write_string(const char *text)
{
    while (*text != '\0') {
        uart_write_char(*text++);
    }
}

int main(void)
{
    uart_init();
    systick_init();

    uint32_t next_print_ms = 0u;

    for (;;) {
        if ((int32_t)(g_ms_ticks - next_print_ms) >= 0) {
            next_print_ms += 1000u;
            uart_write_string("hello\r\n");
        }

        __WFI(); /* SysTick wakes the core; output is due every 1000 ticks. */
    }
}
