#include "stm32f4xx.h"
#include "clock.h"

/*
 * Numbers (limits from RM0390 6.3.2, p.129-130, and 6.3.3):
 *
 *   VCO in  = HSI / M = 16 MHz / 8   = 2 MHz     (must be 1..2 MHz; 2 limits jitter)
 *   VCO out = VCO in x N = 2 x 180   = 360 MHz   (must be 100..432 MHz, N 50..432)
 *   SYSCLK  = VCO out / P = 360 / 2  = 180 MHz   (P is 2, 4, 6 or 8)
 *
 * P = 2 is the only choice that works: SYSCLK = 180 MHz needs VCO = 180 x P,
 * and P = 4 would need 720 MHz, above the 432 MHz VCO limit.
 */
#define HSI_HZ          16000000u
#define PLL_M           8u
#define PLL_N           180u
#define PLL_P           2u                  /* PLLP field value 00 */
#define AHB_DIV         1u
#define APB1_DIV        4u
#define APB2_DIV        2u
#define FLASH_WS        5u                  /* 150 < HCLK <= 180 MHz at 2.7-3.6 V, RM0390 Table 5 */

#define VCO_IN_HZ       (HSI_HZ / PLL_M)
#define VCO_OUT_HZ      (VCO_IN_HZ * PLL_N)
#define SYSCLK_HZ       (VCO_OUT_HZ / PLL_P)
#define PCLK1_HZ        (SYSCLK_HZ / AHB_DIV / APB1_DIV)
#define PCLK2_HZ        (SYSCLK_HZ / AHB_DIV / APB2_DIV)

_Static_assert(HSI_HZ % PLL_M == 0u, "M must divide HSI exactly");
_Static_assert(VCO_IN_HZ >= 1000000u && VCO_IN_HZ <= 2000000u, "VCO input must be 1..2 MHz");
_Static_assert(PLL_N >= 50u && PLL_N <= 432u, "N out of range");
_Static_assert(VCO_OUT_HZ >= 100000000u && VCO_OUT_HZ <= 432000000u, "VCO output must be 100..432 MHz");
_Static_assert(SYSCLK_HZ == 180000000u, "expected 180 MHz");
_Static_assert(PCLK1_HZ <= 45000000u, "APB1 limit is 45 MHz");
_Static_assert(PCLK2_HZ <= 90000000u, "APB2 limit is 90 MHz");

/* RCC_CFGR codes for the dividers above (RM0390 6.3.3, p.132). */
#define CFGR_HPRE_DIV   RCC_CFGR_HPRE_DIV1
#define CFGR_PPRE1_DIV  RCC_CFGR_PPRE1_DIV4
#define CFGR_PPRE2_DIV  RCC_CFGR_PPRE2_DIV2

/* The codes must match the numeric dividers used for the frequencies above.
   A divider not listed here maps to ~0u, so it fails the assert. */
#define PPRE1_CODE(d)   ((d) == 1u ? RCC_CFGR_PPRE1_DIV1 : (d) == 2u ? RCC_CFGR_PPRE1_DIV2 : \
                         (d) == 4u ? RCC_CFGR_PPRE1_DIV4 : ~0u)
#define PPRE2_CODE(d)   ((d) == 1u ? RCC_CFGR_PPRE2_DIV1 : (d) == 2u ? RCC_CFGR_PPRE2_DIV2 : \
                         (d) == 4u ? RCC_CFGR_PPRE2_DIV4 : ~0u)

_Static_assert(CFGR_HPRE_DIV == (AHB_DIV == 1u ? RCC_CFGR_HPRE_DIV1 : ~0u), "HPRE code does not match AHB_DIV");
_Static_assert(CFGR_PPRE1_DIV == PPRE1_CODE(APB1_DIV), "PPRE1 code does not match APB1_DIV");
_Static_assert(CFGR_PPRE2_DIV == PPRE2_CODE(APB2_DIV), "PPRE2 code does not match APB2_DIV");

/* Timers count from 2x their bus clock when the APB prescaler is more than 1
   (RM0390 6.2, Figure 14 note 2; TIMPRE is left at its reset value 0). */
#define TIM_APB1_HZ     ((APB1_DIV == 1u) ? PCLK1_HZ : 2u * PCLK1_HZ)

/* Reset state: HSI, every divider /1. Updated at the end of clock_init(). */
static uint32_t g_sysclk_hz = HSI_HZ;
static uint32_t g_pclk1_hz = HSI_HZ;
static uint32_t g_pclk2_hz = HSI_HZ;
static uint32_t g_tim_apb1_hz = HSI_HZ;

/* A flag that never sets hangs the board here. Recovery: flash again with
   connect-under-reset (make flash does this). */
static void wait_set(volatile uint32_t *reg, uint32_t mask)
{
    while ((*reg & mask) == 0u) {
    }
}

void clock_init(void)
{
    if ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSI) {
        return;                         /* already switched: PLLCFGR is locked while the PLL runs */
    }

    /* The PWR registers sit behind their own clock gate (closed at reset). */
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    (void)RCC->APB1ENR;                 /* read back: lets the gate open before the first PWR access */

    /* Highest voltage scale (the reset value, set here so the code does not
       depend on it). Only writable while the PLL is off; it takes effect when
       the PLL turns on (RM0390 5.1.3). */
    PWR->CR |= PWR_CR_VOS;

    /* Change M, N, P and the source only. PLLQ and PLLR keep their reset
       values: 0 is a "wrong configuration" for both (RM0390 p.129). */
    RCC->PLLCFGR = (RCC->PLLCFGR & ~(RCC_PLLCFGR_PLLM | RCC_PLLCFGR_PLLN |
                                     RCC_PLLCFGR_PLLP | RCC_PLLCFGR_PLLSRC)) |
                   (PLL_M << RCC_PLLCFGR_PLLM_Pos) |
                   (PLL_N << RCC_PLLCFGR_PLLN_Pos) |
                   RCC_PLLCFGR_PLLSRC_HSI;          /* PLLP field 00 = /2 stays cleared */
    RCC->CR |= RCC_CR_PLLON;

    /* Over-drive: 168 -> 180 MHz needs it (RM0390 3.4.1). Start it while the
       PLL locks, as RM0390 5.1.3 recommends. The CPU stalls during the switch. */
    PWR->CR |= PWR_CR_ODEN;
    wait_set(&PWR->CSR, PWR_CSR_ODRDY);
    PWR->CR |= PWR_CR_ODSWEN;
    wait_set(&PWR->CSR, PWR_CSR_ODSWRDY);

    /* Flash first: at 180 MHz a read takes 6 CPU cycles. Switching with the
       reset value (0 wait states) would feed the CPU half-read instructions.
       Prefetch and both caches hide most of the wait (RM0390 3.4.2). */
    FLASH->ACR = (FLASH->ACR & ~FLASH_ACR_LATENCY) | FLASH_ACR_LATENCY_5WS |
                 FLASH_ACR_PRFTEN | FLASH_ACR_ICEN | FLASH_ACR_DCEN;
    while ((FLASH->ACR & FLASH_ACR_LATENCY) != FLASH_WS) {
        /* the new wait states may not apply at once (RM0390 3.4.1) */
    }

    /* Bus dividers before the switch, so APB1 never sees more than 45 MHz. */
    RCC->CFGR = (RCC->CFGR & ~(RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2)) |
                CFGR_HPRE_DIV | CFGR_PPRE1_DIV | CFGR_PPRE2_DIV;

    wait_set(&RCC->CR, RCC_CR_PLLRDY);

    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) {
    }

    g_sysclk_hz = SYSCLK_HZ;
    g_pclk1_hz = PCLK1_HZ;
    g_pclk2_hz = PCLK2_HZ;
    g_tim_apb1_hz = TIM_APB1_HZ;
}

uint32_t clock_sysclk_hz(void)
{
    return g_sysclk_hz;
}

uint32_t clock_pclk1_hz(void)
{
    return g_pclk1_hz;
}

uint32_t clock_pclk2_hz(void)
{
    return g_pclk2_hz;
}

uint32_t clock_tim_apb1_hz(void)
{
    return g_tim_apb1_hz;
}
