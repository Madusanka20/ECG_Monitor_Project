#include "main.h"

/* USER CODE BEGIN Includes */
#include "ads1293.h"
#include "ble_ecg.h"
#include "hci_tl_interface.h"
/* USER CODE END Includes */

/* USER CODE BEGIN PD */
_Static_assert(ADS1293_DRDYB_Pin == GPIO_PIN_0,
               "Expected ADS1293 DRDYB on PB0");
_Static_assert(ADS1293_ALARM_Pin == GPIO_PIN_1,
               "Expected ADS1293 ALARM on PB1");
_Static_assert(BLUENRG_CS_Pin == GPIO_PIN_10,
               "Expected BlueNRG chip select on PA10");
/* USER CODE END PD */

SPI_HandleTypeDef hspi1;

/* USER CODE BEGIN PV */
volatile int32_t ECG_Lead_I;
volatile int32_t ECG_Lead_II;
volatile int32_t ECG_Lead_V1;
volatile uint32_t drdy_count;
volatile uint32_t samples_ok;
volatile uint32_t spi_errors;
volatile uint32_t adc_restarts;
volatile uint8_t adc_ready;

extern volatile uint32_t ble_pkts_sent;

static volatile uint32_t last_sample_ms;
static volatile uint8_t ble_irq_seen;
/* USER CODE END PV */

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);

/* USER CODE BEGIN PFP */
static void start_adc(void);
static void check_adc_timeout(void);
static void sleep_if_idle(uint32_t drdy_before, uint32_t packets_before);
/* USER CODE END PFP */

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_SPI1_Init();

    /* USER CODE BEGIN 2 */
    SCB->SCR &= ~(SCB_SCR_SLEEPDEEP_Msk | SCB_SCR_SLEEPONEXIT_Msk);
    BLE_ECG_Init();
    start_adc();
    /* USER CODE END 2 */

    /* USER CODE BEGIN WHILE */
    while (1)
    {
        /* USER CODE END WHILE */

        /* USER CODE BEGIN 3 */
        ble_irq_seen = 0U;

        uint32_t drdy_before = drdy_count;
        uint32_t packets_before = ble_pkts_sent;

        BLE_ECG_Process();
        check_adc_timeout();
        sleep_if_idle(drdy_before, packets_before);
    }
    /* USER CODE END 3 */
}

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef oscillator = {0};
    RCC_ClkInitTypeDef clock = {0};

    if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE4)
        != HAL_OK)
    {
        Error_Handler();
    }

    oscillator.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    oscillator.HSEState = RCC_HSE_ON;
    oscillator.PLL.PLLState = RCC_PLL_NONE;

    if (HAL_RCC_OscConfig(&oscillator) != HAL_OK)
    {
        Error_Handler();
    }

    clock.ClockType = RCC_CLOCKTYPE_SYSCLK
                    | RCC_CLOCKTYPE_HCLK
                    | RCC_CLOCKTYPE_PCLK1
                    | RCC_CLOCKTYPE_PCLK2
                    | RCC_CLOCKTYPE_PCLK3;
    clock.SYSCLKSource = RCC_SYSCLKSOURCE_HSE;
    clock.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clock.APB1CLKDivider = RCC_HCLK_DIV1;
    clock.APB2CLKDivider = RCC_HCLK_DIV1;
    clock.APB3CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&clock, FLASH_LATENCY_0) != HAL_OK)
    {
        Error_Handler();
    }
}

static void MX_SPI1_Init(void)
{
    SPI_AutonomousModeConfTypeDef autonomous = {0};

    hspi1.Instance = SPI1;
    hspi1.Init.Mode = SPI_MODE_MASTER;
    hspi1.Init.Direction = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi1.Init.NSS = SPI_NSS_SOFT;
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
    hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi1.Init.CRCPolynomial = 0x7;
    hspi1.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;
    hspi1.Init.NSSPolarity = SPI_NSS_POLARITY_LOW;
    hspi1.Init.FifoThreshold = SPI_FIFO_THRESHOLD_01DATA;
    hspi1.Init.MasterSSIdleness = SPI_MASTER_SS_IDLENESS_00CYCLE;
    hspi1.Init.MasterInterDataIdleness =
        SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;
    hspi1.Init.MasterReceiverAutoSusp = SPI_MASTER_RX_AUTOSUSP_DISABLE;
    hspi1.Init.MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_DISABLE;
    hspi1.Init.IOSwap = SPI_IO_SWAP_DISABLE;
    hspi1.Init.ReadyMasterManagement = SPI_RDY_MASTER_MANAGEMENT_INTERNALLY;
    hspi1.Init.ReadyPolarity = SPI_RDY_POLARITY_HIGH;

    if (HAL_SPI_Init(&hspi1) != HAL_OK)
    {
        Error_Handler();
    }

    autonomous.TriggerState = SPI_AUTO_MODE_DISABLE;
    autonomous.TriggerSelection = SPI_GRP1_GPDMA_CH0_TCF_TRG;
    autonomous.TriggerPolarity = SPI_TRIG_POLARITY_RISING;

    if (HAL_SPIEx_SetConfigAutonomousMode(&hspi1, &autonomous) != HAL_OK)
    {
        Error_Handler();
    }
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOH_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* PA9 stays high on the original trace to BlueNRG pad 18. */
    const uint16_t outputs_a = ADS1293_RSTB_Pin
                             | ADS1293_CS_Pin
                             | GPIO_PIN_9
                             | BLUENRG_CS_Pin;

    HAL_GPIO_WritePin(GPIOA, outputs_a, GPIO_PIN_SET);
    HAL_GPIO_WritePin(BLUENRG_RESETN_GPIO_Port,
                      BLUENRG_RESETN_Pin, GPIO_PIN_RESET);

    gpio.Pin = outputs_a;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &gpio);

    gpio.Pin = BLUENRG_RESETN_Pin;
    HAL_GPIO_Init(BLUENRG_RESETN_GPIO_Port, &gpio);

    gpio.Pin = ADS1293_ALARM_Pin;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(ADS1293_ALARM_GPIO_Port, &gpio);

    gpio.Pin = ADS1293_DRDYB_Pin;
    gpio.Mode = GPIO_MODE_IT_FALLING;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(ADS1293_DRDYB_GPIO_Port, &gpio);

    gpio.Pin = BLUENRG_IRQ_Pin;
    gpio.Mode = GPIO_MODE_IT_RISING;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(BLUENRG_IRQ_GPIO_Port, &gpio);

    /* USER CODE BEGIN MX_GPIO_Init_2 */
    HAL_NVIC_DisableIRQ(ADS1293_DRDYB_EXTI_IRQn);
    HAL_NVIC_DisableIRQ(BLUENRG_IRQ_EXTI_IRQn);

    HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);
    HAL_NVIC_SetPriority(ADS1293_DRDYB_EXTI_IRQn, 1, 0);
    HAL_NVIC_SetPriority(BLUENRG_IRQ_EXTI_IRQn, 3, 0);
    /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
static void start_adc(void)
{
    HAL_NVIC_DisableIRQ(ADS1293_DRDYB_EXTI_IRQn);
    adc_ready = 0U;

    HAL_StatusTypeDef status = ADS1293_Init(&hspi1);
    last_sample_ms = HAL_GetTick();

    if (status != HAL_OK)
    {
        return;
    }

    __HAL_GPIO_EXTI_CLEAR_FALLING_IT(ADS1293_DRDYB_Pin);
    HAL_NVIC_ClearPendingIRQ(ADS1293_DRDYB_EXTI_IRQn);
    adc_ready = 1U;

    /* Service a sample already waiting when acquisition starts. */
    if (HAL_GPIO_ReadPin(ADS1293_DRDYB_GPIO_Port, ADS1293_DRDYB_Pin)
            == GPIO_PIN_RESET &&
        __HAL_GPIO_EXTI_GET_FALLING_IT(ADS1293_DRDYB_Pin) == 0U)
    {
        HAL_GPIO_EXTI_Falling_Callback(ADS1293_DRDYB_Pin);
    }

    HAL_NVIC_EnableIRQ(ADS1293_DRDYB_EXTI_IRQn);
}

static void check_adc_timeout(void)
{
    uint32_t irq_state = __get_PRIMASK();
    __disable_irq();

    uint32_t now_ms = HAL_GetTick();
    uint32_t sample_ms = last_sample_ms;

    __set_PRIMASK(irq_state);

    if ((now_ms - sample_ms) > 1000U)
    {
        ++adc_restarts;
        BLE_ECG_MarkGap();
        start_adc();
    }
}

static void sleep_if_idle(uint32_t drdy_before, uint32_t packets_before)
{
    uint32_t irq_state = __get_PRIMASK();
    __disable_irq();

    if (ble_irq_seen == 0U &&
        drdy_count == drdy_before &&
        ble_pkts_sent == packets_before)
    {
        __DSB();
        __WFI();
        __ISB();
    }

    __set_PRIMASK(irq_state);
}

void HAL_GPIO_EXTI_Falling_Callback(uint16_t pin)
{
    if (pin != ADS1293_DRDYB_Pin || adc_ready == 0U)
    {
        return;
    }

    int32_t lead_i;
    int32_t lead_ii;
    int32_t lead_v1;
    uint32_t index = drdy_count++;

    if (ADS1293_ReadData(&lead_i, &lead_ii, &lead_v1) != HAL_OK)
    {
        ++spi_errors;
        return;
    }

    ECG_Lead_I = lead_i;
    ECG_Lead_II = lead_ii;
    ECG_Lead_V1 = lead_v1;

    last_sample_ms = HAL_GetTick();
    ++samples_ok;

    BLE_ECG_PushSample(index, lead_i, lead_ii, lead_v1);
}

void HAL_GPIO_EXTI_Rising_Callback(uint16_t pin)
{
    if (pin == BLUENRG_IRQ_Pin)
    {
        ble_irq_seen = 1U;
        hci_tl_lowlevel_isr();
    }
}
/* USER CODE END 4 */

void Error_Handler(void)
{
    __disable_irq();

    while (1)
    {
    }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file;
    (void)line;
}
#endif
