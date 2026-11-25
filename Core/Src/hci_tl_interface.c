#include "main.h"
#include "custom_bus.h"
#include "hci_tl.h"
#include "hci_tl_interface.h"

#include <stddef.h>
#include <stdint.h>

#define BLE_HEADER_SIZE         5U
#define BLE_HEADER_TIMEOUT_MS   3U
#define BLE_PAYLOAD_TIMEOUT_MS  50U

static uint8_t zeros[HCI_READ_PACKET_SIZE];
static uint8_t discard[HCI_MAX_PAYLOAD_SIZE];

static volatile uint8_t transport_ready;
static volatile int32_t last_read;

volatile uint32_t ble_io_errors;
volatile uint32_t ble_valid_frames;
volatile uint8_t ble_irq_in_reset;
volatile uint8_t ble_last_header[BLE_HEADER_SIZE];

volatile uint16_t ble_read_size;
volatile uint16_t ble_read_capacity;
volatile uint32_t ble_read_length_errors;
volatile uint32_t ble_read_attempts;
volatile uint32_t ble_read_timeouts;
volatile uint32_t ble_read_hal_errors;
volatile uint32_t ble_read_unexpected_frames;

volatile uint8_t ble_write_header[BLE_HEADER_SIZE];
volatile uint8_t ble_write_header_valid;
volatile uint8_t ble_write_result;
volatile uint16_t ble_write_size;
volatile uint16_t ble_write_space;

enum
{
    WRITE_IDLE = 0,
    WRITE_OK = 1,
    WRITE_BUS_UNAVAILABLE = 2,
    WRITE_INVALID_SIZE = 3,
    WRITE_IRQ_TIMEOUT = 4,
    WRITE_HEADER_ERROR = 5,
    WRITE_ALL_FF = 6,
    WRITE_NO_SPACE = 7,
    WRITE_PAYLOAD_ERROR = 8
};

static void select_module(GPIO_PinState level)
{
    HAL_GPIO_WritePin(BLUENRG_CS_GPIO_Port, BLUENRG_CS_Pin, level);
}

static int irq_high(void)
{
    return HAL_GPIO_ReadPin(BLUENRG_IRQ_GPIO_Port, BLUENRG_IRQ_Pin)
           == GPIO_PIN_SET;
}

static int wait_irq(int level, uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();

    while (irq_high() != level)
    {
        if ((HAL_GetTick() - start) >= timeout_ms)
        {
            return 0;
        }
    }

    return 1;
}

static uint16_t read_u16_le(const uint8_t *bytes)
{
    return (uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8);
}

static int header(uint8_t operation, uint8_t reply[BLE_HEADER_SIZE])
{
    uint8_t command[BLE_HEADER_SIZE] = {operation, 0U, 0U, 0U, 0U};

    HAL_StatusTypeDef status = HAL_SPI_TransmitReceive(
        &hspi2, command, reply, BLE_HEADER_SIZE, BLE_HEADER_TIMEOUT_MS);

    if (status != HAL_OK)
    {
        return 0;
    }

    for (unsigned int i = 0; i < BLE_HEADER_SIZE; ++i)
    {
        ble_last_header[i] = reply[i];
    }

    return 1;
}

static int32_t bus_init(void *unused)
{
    (void)unused;

    select_module(GPIO_PIN_SET);
    transport_ready = (BSP_SPI2_Init() == BSP_ERROR_NONE);

    if (!transport_ready)
    {
        ++ble_io_errors;
        return -1;
    }

    return 0;
}

static int32_t reset_module(void)
{
    if (!transport_ready)
    {
        return -1;
    }

    HAL_NVIC_DisableIRQ(EXTI8_IRQn);
    select_module(GPIO_PIN_SET);

    HAL_GPIO_WritePin(BLUENRG_RESETN_GPIO_Port,
                      BLUENRG_RESETN_Pin, GPIO_PIN_RESET);
    HAL_Delay(10U);

    ble_irq_in_reset = (uint8_t)irq_high();
    uint32_t frames_before_reset = ble_valid_frames;

    __HAL_GPIO_EXTI_CLEAR_RISING_IT(BLUENRG_IRQ_Pin);
    HAL_NVIC_ClearPendingIRQ(EXTI8_IRQn);
    HAL_NVIC_EnableIRQ(EXTI8_IRQn);

    HAL_GPIO_WritePin(BLUENRG_RESETN_GPIO_Port,
                      BLUENRG_RESETN_Pin, GPIO_PIN_SET);

    uint32_t start = HAL_GetTick();

    while (ble_valid_frames == frames_before_reset &&
           (HAL_GetTick() - start) < 2000U)
    {
        BLE_TransportPoll();
    }

    return 0;
}

static int32_t receive(uint8_t *buffer, uint16_t capacity)
{
    uint8_t rx[BLE_HEADER_SIZE];
    last_read = 0;

    if (!transport_ready || buffer == NULL || !irq_high())
    {
        return 0;
    }

    select_module(GPIO_PIN_RESET);

    if (!header(0x0BU, rx))
    {
        ++ble_io_errors;
    }
    else
    {
        uint16_t size = read_u16_le(&rx[3]);
        ble_read_size = size;
        ble_read_capacity = capacity;

        if (size > capacity || size > sizeof(zeros))
        {
            ++ble_read_length_errors;
            ++ble_io_errors;
        }
        else if (size != 0U)
        {
            ++ble_read_attempts;

            HAL_StatusTypeDef status = HAL_SPI_TransmitReceive(
                &hspi2, zeros, buffer, size, BLE_PAYLOAD_TIMEOUT_MS);

            if (status == HAL_OK)
            {
                last_read = size;

                if (size >= 3U &&
                    buffer[0] == 0x04U &&
                    size == (uint16_t)(3U + buffer[2]))
                {
                    ++ble_valid_frames;
                }
                else
                {
                    ++ble_read_unexpected_frames;
                }
            }
            else
            {
                ++ble_io_errors;

                if (status == HAL_TIMEOUT)
                {
                    ++ble_read_timeouts;
                }
                else
                {
                    ++ble_read_hal_errors;
                }
            }
        }
    }

    (void)wait_irq(0, 2U);
    select_module(GPIO_PIN_SET);

    return last_read;
}

static uint8_t write_attempt(uint8_t *data, uint16_t size)
{
    uint8_t rx[BLE_HEADER_SIZE];
    unsigned int ff_count = 0U;

    ble_write_header_valid = 0U;
    ble_write_space = 0U;

    if (!wait_irq(1, 10U))
    {
        return WRITE_IRQ_TIMEOUT;
    }

    if (!header(0x0AU, rx))
    {
        return WRITE_HEADER_ERROR;
    }

    for (unsigned int i = 0; i < BLE_HEADER_SIZE; ++i)
    {
        ble_write_header[i] = rx[i];

        if (rx[i] == 0xFFU)
        {
            ++ff_count;
        }
    }

    ble_write_header_valid = 1U;

    if (ff_count == BLE_HEADER_SIZE)
    {
        return WRITE_ALL_FF;
    }

    ble_write_space = read_u16_le(&rx[1]);

    if (ble_write_space < size)
    {
        return WRITE_NO_SPACE;
    }

    HAL_StatusTypeDef status = HAL_SPI_TransmitReceive(
        &hspi2, data, discard, size, BLE_PAYLOAD_TIMEOUT_MS);

    return status == HAL_OK ? WRITE_OK : WRITE_PAYLOAD_ERROR;
}

static int32_t send(uint8_t *data, uint16_t size)
{
    ble_write_size = size;
    ble_write_header_valid = 0U;
    ble_write_space = 0U;
    ble_write_result = WRITE_IDLE;

    if (!transport_ready)
    {
        ble_write_result = WRITE_BUS_UNAVAILABLE;
        ++ble_io_errors;
        return -1;
    }

    if (data == NULL || size == 0U || size > sizeof(discard))
    {
        ble_write_result = WRITE_INVALID_SIZE;
        ++ble_io_errors;
        return -1;
    }

    uint32_t start = HAL_GetTick();
    uint8_t result;

    HAL_NVIC_DisableIRQ(EXTI8_IRQn);

    do
    {
        select_module(GPIO_PIN_RESET);
        result = write_attempt(data, size);
        select_module(GPIO_PIN_SET);

        if (result != WRITE_NO_SPACE)
        {
            break;
        }
    } while ((HAL_GetTick() - start) < 20U);

    ble_write_result = result;

    if (result != WRITE_OK)
    {
        ++ble_io_errors;
    }

    (void)wait_irq(0, 2U);
    HAL_NVIC_EnableIRQ(EXTI8_IRQn);
    BLE_TransportPoll();

    return result == WRITE_OK ? 0 : -1;
}

void hci_tl_lowlevel_init(void)
{
    tHciIO io = {0};

    io.Init = bus_init;
    io.Reset = reset_module;
    io.Receive = receive;
    io.Send = send;
    io.GetTick = BSP_GetTick;

    hci_register_io_bus(&io);
}

void hci_tl_lowlevel_isr(void)
{
    for (unsigned int i = 0; i < 6U && transport_ready && irq_high(); ++i)
    {
        if (hci_notify_asynch_evt(NULL) || last_read <= 0)
        {
            break;
        }
    }
}

void BLE_TransportPoll(void)
{
    if (transport_ready && irq_high())
    {
        HAL_NVIC_DisableIRQ(EXTI8_IRQn);
        hci_tl_lowlevel_isr();
        HAL_NVIC_EnableIRQ(EXTI8_IRQn);
    }
}
