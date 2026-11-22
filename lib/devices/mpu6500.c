#include "delay.h"
#include "mpu6500.h"
#include "spi.h"

#define ADDR_READ           0x80u

/* PWR_MGMT_1 (p.41) and SIGNAL_PATH_RESET (p.39) */
#define DEVICE_RESET        0x80u
#define PATH_RESET_ALL      0x07u               /* GYRO_RST | ACCEL_RST | TEMP_RST */
#define I2C_IF_DIS          0x10u               /* USER_CTRL bit 4 (p.40) */
#define RESET_WAIT_MS       100u                /* both waits are 100 ms (p.42) */

uint8_t mpu6500_read(uint8_t reg)
{
    spi_cs_low();
    (void)spi_xfer((uint8_t)(ADDR_READ | reg));
    uint8_t value = spi_xfer(0x00u);
    spi_cs_high();
    return value;
}

void mpu6500_write(uint8_t reg, uint8_t value)
{
    spi_cs_low();
    (void)spi_xfer(reg);
    (void)spi_xfer(value);
    spi_cs_high();
}

void mpu6500_reset(void)
{
    mpu6500_write(MPU6500_REG_PWR_MGMT_1, DEVICE_RESET);
    delay_ms(RESET_WAIT_MS);
    mpu6500_write(MPU6500_REG_SIGNAL_PATH_RESET, PATH_RESET_ALL);
    delay_ms(RESET_WAIT_MS);
    mpu6500_write(MPU6500_REG_USER_CTRL, I2C_IF_DIS);
}
