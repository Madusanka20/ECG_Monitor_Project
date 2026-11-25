/* USER CODE BEGIN Header */
/*
 * Copyright (c) 2026 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 */
/* USER CODE END Header */

#include "custom_bus.h"

__weak HAL_StatusTypeDef MX_SPI2_Init(SPI_HandleTypeDef *hspi);

SPI_HandleTypeDef hspi2;

#if (USE_HAL_SPI_REGISTER_CALLBACKS == 1U)
static uint32_t IsSPI2MspCbValid = 0U;
#endif

static uint32_t SPI2InitCounter = 0U;

static void SPI2_MspInit(SPI_HandleTypeDef *spiHandle);
static void SPI2_MspDeInit(SPI_HandleTypeDef *spiHandle);

#if (USE_CUBEMX_BSP_V2 == 1)
static uint32_t SPI_GetPrescaler(uint32_t clock_src_hz, uint32_t baudrate_mbps);
#endif

int32_t BSP_SPI2_Init(void)
{
    int32_t ret = BSP_ERROR_NONE;

    hspi2.Instance = SPI2;

    if (SPI2InitCounter++ == 0U)
    {
        if (HAL_SPI_GetState(&hspi2) == HAL_SPI_STATE_RESET)
        {
#if (USE_HAL_SPI_REGISTER_CALLBACKS == 0U)
            SPI2_MspInit(&hspi2);
#else
            if (IsSPI2MspCbValid == 0U)
            {
                if (BSP_SPI2_RegisterDefaultMspCallbacks() != BSP_ERROR_NONE)
                {
                    return BSP_ERROR_MSP_FAILURE;
                }
            }
#endif

            if (MX_SPI2_Init(&hspi2) != HAL_OK)
            {
                ret = BSP_ERROR_BUS_FAILURE;
            }
        }
    }

    return ret;
}

int32_t BSP_SPI2_DeInit(void)
{
    int32_t ret = BSP_ERROR_BUS_FAILURE;

    if (SPI2InitCounter > 0U)
    {
        if (--SPI2InitCounter == 0U)
        {
#if (USE_HAL_SPI_REGISTER_CALLBACKS == 0U)
            SPI2_MspDeInit(&hspi2);
#endif

            if (HAL_SPI_DeInit(&hspi2) == HAL_OK)
            {
                ret = BSP_ERROR_NONE;
            }
        }
    }

    return ret;
}

int32_t BSP_SPI2_Send(uint8_t *pData, uint16_t Length)
{
    if (HAL_SPI_Transmit(&hspi2, pData, Length, BUS_SPI2_POLL_TIMEOUT)
        != HAL_OK)
    {
        return BSP_ERROR_UNKNOWN_FAILURE;
    }

    return BSP_ERROR_NONE;
}

int32_t BSP_SPI2_Recv(uint8_t *pData, uint16_t Length)
{
    if (HAL_SPI_Receive(&hspi2, pData, Length, BUS_SPI2_POLL_TIMEOUT)
        != HAL_OK)
    {
        return BSP_ERROR_UNKNOWN_FAILURE;
    }

    return BSP_ERROR_NONE;
}

int32_t BSP_SPI2_SendRecv(uint8_t *pTxData, uint8_t *pRxData, uint16_t Length)
{
    if (HAL_SPI_TransmitReceive(&hspi2, pTxData, pRxData, Length,
                                BUS_SPI2_POLL_TIMEOUT) != HAL_OK)
    {
        return BSP_ERROR_UNKNOWN_FAILURE;
    }

    return BSP_ERROR_NONE;
}

#if (USE_HAL_SPI_REGISTER_CALLBACKS == 1U)

int32_t BSP_SPI2_RegisterDefaultMspCallbacks(void)
{
    __HAL_SPI_RESET_HANDLE_STATE(&hspi2);

    if (HAL_SPI_RegisterCallback(&hspi2, HAL_SPI_MSPINIT_CB_ID,
                                 SPI2_MspInit) != HAL_OK)
    {
        return BSP_ERROR_PERIPH_FAILURE;
    }

    if (HAL_SPI_RegisterCallback(&hspi2, HAL_SPI_MSPDEINIT_CB_ID,
                                 SPI2_MspDeInit) != HAL_OK)
    {
        return BSP_ERROR_PERIPH_FAILURE;
    }

    IsSPI2MspCbValid = 1U;
    return BSP_ERROR_NONE;
}

int32_t BSP_SPI2_RegisterMspCallbacks(BSP_SPI_Cb_t *Callbacks)
{
    __HAL_SPI_RESET_HANDLE_STATE(&hspi2);

    if (HAL_SPI_RegisterCallback(&hspi2, HAL_SPI_MSPINIT_CB_ID,
                                 Callbacks->pMspInitCb) != HAL_OK)
    {
        return BSP_ERROR_PERIPH_FAILURE;
    }

    if (HAL_SPI_RegisterCallback(&hspi2, HAL_SPI_MSPDEINIT_CB_ID,
                                 Callbacks->pMspDeInitCb) != HAL_OK)
    {
        return BSP_ERROR_PERIPH_FAILURE;
    }

    IsSPI2MspCbValid = 1U;
    return BSP_ERROR_NONE;
}

#endif

int32_t BSP_GetTick(void)
{
    return (int32_t)HAL_GetTick();
}

__weak HAL_StatusTypeDef MX_SPI2_Init(SPI_HandleTypeDef *hspi)
{
    HAL_StatusTypeDef ret = HAL_OK;
    SPI_AutonomousModeConfTypeDef autonomous = {0};

    hspi->Instance = SPI2;
    hspi->Init.Mode = SPI_MODE_MASTER;
    hspi->Init.Direction = SPI_DIRECTION_2LINES;
    hspi->Init.DataSize = SPI_DATASIZE_8BIT;
    hspi->Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi->Init.CLKPhase = SPI_PHASE_2EDGE;
    hspi->Init.NSS = SPI_NSS_SOFT;
    hspi->Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
    hspi->Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi->Init.TIMode = SPI_TIMODE_DISABLE;
    hspi->Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi->Init.CRCPolynomial = 0x7;
    hspi->Init.NSSPMode = SPI_NSS_PULSE_ENABLE;
    hspi->Init.NSSPolarity = SPI_NSS_POLARITY_LOW;
    hspi->Init.FifoThreshold = SPI_FIFO_THRESHOLD_01DATA;
    hspi->Init.MasterSSIdleness = SPI_MASTER_SS_IDLENESS_00CYCLE;
    hspi->Init.MasterInterDataIdleness = SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;
    hspi->Init.MasterReceiverAutoSusp = SPI_MASTER_RX_AUTOSUSP_DISABLE;
    hspi->Init.MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_DISABLE;
    hspi->Init.IOSwap = SPI_IO_SWAP_DISABLE;
    hspi->Init.ReadyMasterManagement = SPI_RDY_MASTER_MANAGEMENT_INTERNALLY;
    hspi->Init.ReadyPolarity = SPI_RDY_POLARITY_HIGH;

    if (HAL_SPI_Init(hspi) != HAL_OK)
    {
        ret = HAL_ERROR;
    }

    autonomous.TriggerState = SPI_AUTO_MODE_DISABLE;
    autonomous.TriggerSelection = SPI_GRP1_GPDMA_CH0_TCF_TRG;
    autonomous.TriggerPolarity = SPI_TRIG_POLARITY_RISING;

    if (HAL_SPIEx_SetConfigAutonomousMode(hspi, &autonomous) != HAL_OK)
    {
        ret = HAL_ERROR;
    }

    return ret;
}

static void SPI2_MspInit(SPI_HandleTypeDef *spiHandle)
{
    GPIO_InitTypeDef gpio = {0};
    RCC_PeriphCLKInitTypeDef clock = {0};

    /* USER CODE BEGIN SPI2_MspInit 0 */
    (void)spiHandle;
    /* USER CODE END SPI2_MspInit 0 */

    clock.PeriphClockSelection = RCC_PERIPHCLK_SPI2;
    clock.Spi2ClockSelection = RCC_SPI2CLKSOURCE_SYSCLK;
    HAL_RCCEx_PeriphCLKConfig(&clock);

    __HAL_RCC_SPI2_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;

    gpio.Pin = BUS_SPI2_SCK_GPIO_PIN;
    gpio.Alternate = BUS_SPI2_SCK_GPIO_AF;
    HAL_GPIO_Init(BUS_SPI2_SCK_GPIO_PORT, &gpio);

    gpio.Pin = BUS_SPI2_MISO_GPIO_PIN;
    gpio.Alternate = BUS_SPI2_MISO_GPIO_AF;
    HAL_GPIO_Init(BUS_SPI2_MISO_GPIO_PORT, &gpio);

    gpio.Pin = BUS_SPI2_MOSI_GPIO_PIN;
    gpio.Alternate = BUS_SPI2_MOSI_GPIO_AF;
    HAL_GPIO_Init(BUS_SPI2_MOSI_GPIO_PORT, &gpio);

    /* USER CODE BEGIN SPI2_MspInit 1 */
    /* USER CODE END SPI2_MspInit 1 */
}

static void SPI2_MspDeInit(SPI_HandleTypeDef *spiHandle)
{
    /* USER CODE BEGIN SPI2_MspDeInit 0 */
    (void)spiHandle;
    /* USER CODE END SPI2_MspDeInit 0 */

    __HAL_RCC_SPI2_CLK_DISABLE();

    HAL_GPIO_DeInit(BUS_SPI2_SCK_GPIO_PORT, BUS_SPI2_SCK_GPIO_PIN);
    HAL_GPIO_DeInit(BUS_SPI2_MISO_GPIO_PORT, BUS_SPI2_MISO_GPIO_PIN);
    HAL_GPIO_DeInit(BUS_SPI2_MOSI_GPIO_PORT, BUS_SPI2_MOSI_GPIO_PIN);

    /* USER CODE BEGIN SPI2_MspDeInit 1 */
    /* USER CODE END SPI2_MspDeInit 1 */
}

#if (USE_CUBEMX_BSP_V2 == 1)

static uint32_t SPI_GetPrescaler(uint32_t clock_src_hz, uint32_t baudrate_mbps)
{
    static const uint32_t baudrate[] =
    {
        SPI_BAUDRATEPRESCALER_2,
        SPI_BAUDRATEPRESCALER_4,
        SPI_BAUDRATEPRESCALER_8,
        SPI_BAUDRATEPRESCALER_16,
        SPI_BAUDRATEPRESCALER_32,
        SPI_BAUDRATEPRESCALER_64,
        SPI_BAUDRATEPRESCALER_128,
        SPI_BAUDRATEPRESCALER_256
    };

    uint32_t divisor = 0U;
    uint32_t spi_clk = clock_src_hz;
    uint32_t prescaler = 0U;

    while (spi_clk > baudrate_mbps)
    {
        prescaler = baudrate[divisor];
        ++divisor;

        if (divisor >= 8U)
        {
            break;
        }

        spi_clk >>= 1;
    }

    return prescaler;
}

#endif
