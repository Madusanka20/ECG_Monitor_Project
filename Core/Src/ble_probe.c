#include "ble_probe.h"

extern SPI_HandleTypeDef hspi2;

volatile uint8_t ble_probe_rx[5] = {0};
volatile uint8_t ble_probe_irq   = 0;
volatile uint8_t ble_probe_ok    = 0;
volatile uint32_t ble_probe_spi_status = 0xFFFFFFFFU;
volatile uint8_t ble_probe_rx2[5] = {0};
volatile uint32_t ble_probe_spi_status2 = 0xFFFFFFFFU;

void BLE_Probe(void)
{
    uint8_t tx[5] = { 0x0B, 0x00, 0x00, 0x00, 0x00 };   /* SPI "read header" */
    uint8_t rx[5] = { 0 };

    /* Hardware reset */
    HAL_GPIO_WritePin(BLUENRG_CS_GPIO_Port, BLUENRG_CS_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(BLUENRG_RESETN_GPIO_Port, BLUENRG_RESETN_Pin, GPIO_PIN_RESET);
    HAL_Delay(5);
    HAL_GPIO_WritePin(BLUENRG_RESETN_GPIO_Port, BLUENRG_RESETN_Pin, GPIO_PIN_SET);
    HAL_Delay(500);                                      /* module boot time */

    ble_probe_irq = (HAL_GPIO_ReadPin(BLUENRG_IRQ_GPIO_Port, BLUENRG_IRQ_Pin) == GPIO_PIN_SET);

    HAL_GPIO_WritePin(BLUENRG_CS_GPIO_Port, BLUENRG_CS_Pin, GPIO_PIN_RESET);

    HAL_StatusTypeDef st = HAL_SPI_TransmitReceive(&hspi2, tx, rx, 5, 100);
    ble_probe_spi_status = (uint32_t)st;
    HAL_GPIO_WritePin(BLUENRG_CS_GPIO_Port, BLUENRG_CS_Pin, GPIO_PIN_SET);

    for (int i = 0; i < 5; i++) ble_probe_rx[i] = rx[i];
    ble_probe_ok = (st == HAL_OK && rx[0] == 0x02);

    HAL_Delay(10);

    uint8_t tx2[5] = {0x0B, 0, 0, 0, 0};
    uint8_t rx2[5] = {0};

    HAL_GPIO_WritePin(BLUENRG_CS_GPIO_Port,
                      BLUENRG_CS_Pin, GPIO_PIN_RESET);

    HAL_StatusTypeDef st2 =
        HAL_SPI_TransmitReceive(&hspi2, tx2, rx2, 5, 100);

    HAL_GPIO_WritePin(BLUENRG_CS_GPIO_Port,
                      BLUENRG_CS_Pin, GPIO_PIN_SET);

    ble_probe_spi_status2 = (uint32_t)st2;

    for (int i = 0; i < 5; i++)
    {
        ble_probe_rx2[i] = rx2[i];
    }
}
