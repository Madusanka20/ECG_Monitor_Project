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

static inline int32_t sign_extend24(const uint8_t *b)
{
    uint32_t v = ((uint32_t)b[0] << 16) | ((uint32_t)b[1] << 8) | b[2];
    return ((int32_t)(v << 8)) >> 8;
}

void ADS1293_Init(SPI_HandleTypeDef *hspi)
{
    uint8_t v;

    ads_spi = hspi;
    ADS1293_Reset();

    /* ECG channel inputs */
    ADS1293_WriteRegister(ADS1293_REG_FLEX_CH1_CN, 0x11);
    ADS1293_WriteRegister(ADS1293_REG_FLEX_CH2_CN, 0x19);
    ADS1293_WriteRegister(ADS1293_REG_FLEX_CH3_CN, 0x2E);

    /* Common mode / RLD */
    ADS1293_WriteRegister(ADS1293_REG_CMDET_EN, 0x07);
    ADS1293_WriteRegister(ADS1293_REG_RLD_CN,   0x04);

    /* Wilson terminal */
    ADS1293_WriteRegister(ADS1293_REG_WILSON_EN1, 0x01);
    ADS1293_WriteRegister(ADS1293_REG_WILSON_EN2, 0x02);
    ADS1293_WriteRegister(ADS1293_REG_WILSON_EN3, 0x03);
    ADS1293_WriteRegister(ADS1293_REG_WILSON_CN,  0x01);

    /* Oscillator / clock source */
    ADS1293_WriteRegister(ADS1293_REG_OSC_CN, 0x04);

    /* Decimation */
    ADS1293_WriteRegister(ADS1293_REG_R2_RATE,  0x02);
    ADS1293_WriteRegister(ADS1293_REG_R3_RATE1, 0x02);
    ADS1293_WriteRegister(ADS1293_REG_R3_RATE2, 0x02);
    ADS1293_WriteRegister(ADS1293_REG_R3_RATE3, 0x02);

    /* CH1 ECG drives DRDYB */
    ADS1293_WriteRegister(ADS1293_REG_DRDYB_SRC, 0x08);

    /* Enable E1, E2, E3 in loop read (no status byte) */
    ADS1293_WriteRegister(ADS1293_REG_CH_CNFG,
                          ADS1293_CH_CNFG_E1_EN |
                          ADS1293_CH_CNFG_E2_EN |
                          ADS1293_CH_CNFG_E3_EN);

    ADS1293_WriteRegister(ADS1293_REG_SYNCOUTB_SRC, 0x00);

    /* Start conversion */
    ADS1293_WriteRegister(ADS1293_REG_CONFIG, 0x01);

    /* Read back for debugging */
    if (ADS1293_ReadRegister(ADS1293_REG_DRDYB_SRC,   &v) == HAL_OK) debug_27 = v;
    if (ADS1293_ReadRegister(ADS1293_REG_CONFIG,      &v) == HAL_OK) debug_00 = v;
    if (ADS1293_ReadRegister(ADS1293_REG_CH_CNFG,     &v) == HAL_OK) debug_2F = v;
    if (ADS1293_ReadRegister(ADS1293_REG_SYNCOUTB_SRC,&v) == HAL_OK) debug_29 = v;
    if (ADS1293_ReadRegister(ADS1293_REG_DATA_STATUS, &v) == HAL_OK) debug_status = v;
}

void ADS1293_Reset(void)
{
    HAL_GPIO_WritePin(ADS1293_RST_PORT, ADS1293_RST_PIN, GPIO_PIN_RESET);
    HAL_Delay(2);
    HAL_GPIO_WritePin(ADS1293_RST_PORT, ADS1293_RST_PIN, GPIO_PIN_SET);
    HAL_Delay(5);
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
    uint8_t tx[10] = { ADS1293_CMD_READ | ADS1293_REG_DATA_CH1_ECG };
    uint8_t rx[10] = { 0 };

    /* Burst read 0x37..0x3F (address auto-increments) */
    cs_low();
    HAL_StatusTypeDef st = HAL_SPI_TransmitReceive(ads_spi, tx, rx, 10, 10);
    cs_high();
    if (st != HAL_OK) return st;

    *ch1 = sign_extend24(&rx[1]);
    *ch2 = sign_extend24(&rx[4]);
    *ch3 = sign_extend24(&rx[7]);
    return HAL_OK;
}

HAL_StatusTypeDef ADS1293_ReadLoopData(volatile int32_t *ch1,
                                       volatile int32_t *ch2,
                                       volatile int32_t *ch3)
{
    uint8_t tx[10] = { ADS1293_CMD_READ | ADS1293_REG_DATA_LOOP };
    uint8_t rx[10] = { 0 };

    cs_low();
    HAL_StatusTypeDef st = HAL_SPI_TransmitReceive(ads_spi, tx, rx, 10, 10);
    cs_high();
    if (st != HAL_OK) return st;

    *ch1 = sign_extend24(&rx[1]);
    *ch2 = sign_extend24(&rx[4]);
    *ch3 = sign_extend24(&rx[7]);
    return HAL_OK;
}
