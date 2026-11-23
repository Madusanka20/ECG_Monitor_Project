#include "hci_tl.h"
#include "hci_tl_interface.h"

#define HEADER_SIZE       5U
#define MAX_BUFFER_SIZE   255U
#define TIMEOUT_DURATION  50U

extern SPI_HandleTypeDef hspi2;

static int32_t IsDataAvailable(void)
{
  return (HAL_GPIO_ReadPin(HCI_TL_SPI_EXTI_PORT, HCI_TL_SPI_EXTI_PIN) == GPIO_PIN_SET);
}

static int32_t HCI_GetTick(void)
{
  return (int32_t)HAL_GetTick();
}

int32_t HCI_TL_SPI_Init(void* pConf)
{
  (void)pConf;
  HAL_GPIO_WritePin(HCI_TL_SPI_CS_PORT, HCI_TL_SPI_CS_PIN, GPIO_PIN_SET);
  return 0;
}

int32_t HCI_TL_SPI_DeInit(void)
{
  return 0;
}

int32_t HCI_TL_SPI_Reset(void)
{
  HAL_GPIO_WritePin(HCI_TL_SPI_CS_PORT, HCI_TL_SPI_CS_PIN, GPIO_PIN_SET);
  HAL_GPIO_WritePin(HCI_TL_RST_PORT, HCI_TL_RST_PIN, GPIO_PIN_RESET);
  HAL_Delay(10);
  HAL_GPIO_WritePin(HCI_TL_RST_PORT, HCI_TL_RST_PIN, GPIO_PIN_SET);
  HAL_Delay(10);
  return 0;
}

int32_t HCI_TL_SPI_Receive(uint8_t* buffer, uint16_t size)
{
  uint16_t byte_count;
  uint16_t len = 0;
  uint8_t header_master[HEADER_SIZE] = {0x0b, 0x00, 0x00, 0x00, 0x00};
  uint8_t header_slave[HEADER_SIZE]  = {0};
  static uint8_t dummy_tx[MAX_BUFFER_SIZE] = {0};

  HAL_NVIC_DisableIRQ(HCI_TL_SPI_EXTI_IRQn);
  HAL_GPIO_WritePin(HCI_TL_SPI_CS_PORT, HCI_TL_SPI_CS_PIN, GPIO_PIN_RESET);

  HAL_SPI_TransmitReceive(&hspi2, header_master, header_slave, HEADER_SIZE, TIMEOUT_DURATION);

  if (header_slave[0] == 0x02U)
  {
    byte_count = ((uint16_t)header_slave[4] << 8) | header_slave[3];

    if (byte_count > 0U)
    {
      if (byte_count > size)
      {
        byte_count = size;
      }
      if (byte_count > MAX_BUFFER_SIZE)
      {
        byte_count = MAX_BUFFER_SIZE;
      }

      HAL_SPI_TransmitReceive(&hspi2, dummy_tx, buffer, byte_count, TIMEOUT_DURATION);
      len = byte_count;
    }

    uint32_t tickstart = HAL_GetTick();
    while ((HAL_GetTick() - tickstart) < 5U)
    {
      if (HAL_GPIO_ReadPin(HCI_TL_SPI_IRQ_PORT, HCI_TL_SPI_IRQ_PIN) == GPIO_PIN_RESET)
      {
        break;
      }
    }
  }

  HAL_GPIO_WritePin(HCI_TL_SPI_CS_PORT, HCI_TL_SPI_CS_PIN, GPIO_PIN_SET);
  HAL_NVIC_EnableIRQ(HCI_TL_SPI_EXTI_IRQn);

  return (int32_t)len;
}

int32_t HCI_TL_SPI_Send(uint8_t* buffer, uint16_t size)
{
  int32_t result = 0;
  uint16_t rx_bytes;
  uint8_t header_master[HEADER_SIZE] = {0x0a, 0x00, 0x00, 0x00, 0x00};
  uint8_t header_slave[HEADER_SIZE]  = {0};
  static uint8_t read_char_buf[MAX_BUFFER_SIZE];
  uint32_t tickstart = HAL_GetTick();

  do
  {
    uint32_t tickstart_data_available = HAL_GetTick();
    result = 0;

    HAL_GPIO_WritePin(HCI_TL_SPI_CS_PORT, HCI_TL_SPI_CS_PIN, GPIO_PIN_RESET);

    while (!IsDataAvailable())
    {
      if ((HAL_GetTick() - tickstart_data_available) > TIMEOUT_DURATION)
      {
        result = -3;
        break;
      }
    }

    if (result == -3)
    {
      HAL_GPIO_WritePin(HCI_TL_SPI_CS_PORT, HCI_TL_SPI_CS_PIN, GPIO_PIN_SET);
      break;
    }

    HAL_SPI_TransmitReceive(&hspi2, header_master, header_slave, HEADER_SIZE, TIMEOUT_DURATION);
    rx_bytes = (((uint16_t)header_slave[2]) << 8) | ((uint16_t)header_slave[1]);

    if (rx_bytes >= size)
    {
      HAL_SPI_TransmitReceive(&hspi2, buffer, read_char_buf, size, TIMEOUT_DURATION);
    }
    else
    {
      result = -2;
    }

    HAL_GPIO_WritePin(HCI_TL_SPI_CS_PORT, HCI_TL_SPI_CS_PIN, GPIO_PIN_SET);

    if ((HAL_GetTick() - tickstart) > TIMEOUT_DURATION)
    {
      result = -3;
      break;
    }
  } while (result < 0);

  tickstart = HAL_GetTick();
  while ((HAL_GetTick() - tickstart) < 5U)
  {
    if (HAL_GPIO_ReadPin(HCI_TL_SPI_IRQ_PORT, HCI_TL_SPI_IRQ_PIN) == GPIO_PIN_RESET)
    {
      break;
    }
  }

  return result;
}

void hci_tl_lowlevel_init(void)
{
  tHciIO fops;

  fops.Init    = HCI_TL_SPI_Init;
  fops.DeInit  = HCI_TL_SPI_DeInit;
  fops.Send    = HCI_TL_SPI_Send;
  fops.Receive = HCI_TL_SPI_Receive;
  fops.Reset   = HCI_TL_SPI_Reset;
  fops.GetTick = HCI_GetTick;

  hci_register_io_bus(&fops);

  __HAL_GPIO_EXTI_CLEAR_IT(HCI_TL_SPI_EXTI_PIN);
  HAL_NVIC_ClearPendingIRQ(HCI_TL_SPI_EXTI_IRQn);
  HAL_NVIC_EnableIRQ(HCI_TL_SPI_EXTI_IRQn);
}

void hci_tl_lowlevel_isr(void)
{
  while (IsDataAvailable())
  {
    if (hci_notify_asynch_evt(NULL))
    {
      return;
    }
  }
}
