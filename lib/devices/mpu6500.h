#ifndef MPU6500_H
#define MPU6500_H

#include <stdint.h>

/*
 * MPU-6500 over SPI (lib/core/spi.c, chip select PB6). The chip is always a slave.
 * Address byte = R/W bit (1 = read) + 7-bit register address; data follows
 * (product spec 6.5 p.34). SCLK max 1 MHz (Table 7 p.16), so use BR 6 or 7.
 * Register numbers are from the register map (RM-MPU-6500A-00).
 */
#define MPU6500_REG_SIGNAL_PATH_RESET   0x68u   /* 104 */
#define MPU6500_REG_USER_CTRL           0x6Au   /* 106 */
#define MPU6500_REG_PWR_MGMT_1          0x6Bu   /* 107 */
#define MPU6500_REG_WHO_AM_I            0x75u   /* 117, reads 0x70 */

#define MPU6500_WHO_AM_I_VALUE          0x70u

uint8_t mpu6500_read(uint8_t reg);
void mpu6500_write(uint8_t reg, uint8_t value);

/* Reset sequence for SPI (register map p.42), then SPI-only mode (p.40).
   Takes about 200 ms. */
void mpu6500_reset(void);

#endif
