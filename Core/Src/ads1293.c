#include "ads1293.h"

/* Debug read-back variables (owned by this file) */
volatile uint8_t debug_27     = 0;
volatile uint8_t debug_00     = 0;
volatile uint8_t debug_2F     = 0;
volatile uint8_t debug_29     = 0;
volatile uint8_t debug_status = 0;

static SPI_HandleTypeDef *ads_spi;

static inline void cs_low(void)
{
    HAL_GPIO_WritePin(ADS1293_CS_PORT, ADS1293_CS_PIN, GPIO_PIN_RESET);
}

static inline void cs_high(void)
{
    HAL_GPIO_WritePin(ADS1293_CS_PORT, ADS1293_CS_PIN, GPIO_PIN_SET);
}

/* TI Table 8: R1=4, R2=4, R3=16; zero differential input gives 0x400000. */
static int32_t centered_code(const uint8_t *bytes)
{
    int32_t raw = ((int32_t)bytes[0] << 16) | ((int32_t)bytes[1] << 8) | bytes[2];
    return raw - 0x400000;
}

volatile uint8_t adc_failed_reg, adc_expected, adc_actual;

HAL_StatusTypeDef ADS1293_Init(SPI_HandleTypeDef *hspi)
{
    static const uint8_t config[][2] = {
        {ADS1293_REG_FLEX_CH1_CN, 0x11}, {ADS1293_REG_FLEX_CH2_CN, 0x19},
        {ADS1293_REG_FLEX_CH3_CN, 0x2E}, {ADS1293_REG_CMDET_EN, 0x07},
        {ADS1293_REG_RLD_CN, 0x04}, {ADS1293_REG_WILSON_EN1, 0x01},
        {ADS1293_REG_WILSON_EN2, 0x02}, {ADS1293_REG_WILSON_EN3, 0x03},
        {ADS1293_REG_WILSON_CN, 0x01}, {ADS1293_REG_OSC_CN, 0x04},
        {0x13, 0x00}, {0x25, 0x00}, {ADS1293_REG_R2_RATE, 0x01},
        {ADS1293_REG_R3_RATE1, 0x10}, {ADS1293_REG_R3_RATE2, 0x10},
        {ADS1293_REG_R3_RATE3, 0x10}, {ADS1293_REG_DRDYB_SRC, 0x08},
        {ADS1293_REG_CH_CNFG, 0x70}, {ADS1293_REG_MASK_DRDYB, 0x00},
        {ADS1293_REG_CONFIG, 0x01}
    };
    ads_spi = hspi;
    ADS1293_Reset();
    for (unsigned i = 0; i < sizeof(config) / sizeof(config[0]); ++i)
    {
        uint8_t reg = config[i][0], value = config[i][1], actual = 0;
        if (ADS1293_WriteRegister(reg, value) != HAL_OK ||
            ADS1293_ReadRegister(reg, &actual) != HAL_OK || actual != value)
        {
            adc_failed_reg = reg;
            adc_expected = value;
            adc_actual = actual;
            return HAL_ERROR;
        }
        if (reg == ADS1293_REG_DRDYB_SRC) debug_27 = actual;
        if (reg == ADS1293_REG_CONFIG) debug_00 = actual;
        if (reg == ADS1293_REG_CH_CNFG) debug_2F = actual;
        if (reg == ADS1293_REG_MASK_DRDYB) debug_29 = actual;
    }
    adc_failed_reg = 0xFF;
    return HAL_OK;
}

void ADS1293_Reset(void)
{
    HAL_GPIO_WritePin(ADS1293_RST_PORT, ADS1293_RST_PIN, GPIO_PIN_RESET);
    HAL_Delay(2);
    HAL_GPIO_WritePin(ADS1293_RST_PORT, ADS1293_RST_PIN, GPIO_PIN_SET);
    HAL_Delay(50);
}

HAL_StatusTypeDef ADS1293_WriteRegister(uint8_t reg, uint8_t data)
{
    uint8_t buf[2] = { ADS1293_CMD_WRITE | (reg & ADS1293_ADDR_MASK), data };

    cs_low();
    HAL_StatusTypeDef st = HAL_SPI_Transmit(ads_spi, buf, 2, 10);
    cs_high();
    return st;
}

HAL_StatusTypeDef ADS1293_ReadRegister(uint8_t reg, uint8_t *data)
{
    uint8_t tx[2] = { ADS1293_CMD_READ | (reg & ADS1293_ADDR_MASK), 0x00 };
    uint8_t rx[2] = { 0, 0 };

    cs_low();
    HAL_StatusTypeDef st = HAL_SPI_TransmitReceive(ads_spi, tx, rx, 2, 10);
    cs_high();

    if (st == HAL_OK) *data = rx[1];
    return st;
}

HAL_StatusTypeDef ADS1293_ReadData(int32_t *ch1, int32_t *ch2, int32_t *ch3)
{
    return ADS1293_ReadLoopData(ch1, ch2, ch3);
}

HAL_StatusTypeDef ADS1293_ReadLoopData(volatile int32_t *ch1,
                                       volatile int32_t *ch2,
                                       volatile int32_t *ch3)
{
    uint8_t tx[10] = { ADS1293_CMD_READ | ADS1293_REG_DATA_LOOP };
    uint8_t rx[10] = { 0 };

    cs_low();
    HAL_StatusTypeDef st = HAL_SPI_TransmitReceive(ads_spi, tx, rx, 10, 1);
    cs_high();
    if (st != HAL_OK) return st;

    *ch1 = centered_code(&rx[1]);
    *ch2 = centered_code(&rx[4]);
    *ch3 = centered_code(&rx[7]);
    return HAL_OK;
}
