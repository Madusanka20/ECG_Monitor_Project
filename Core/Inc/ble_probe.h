#ifndef BLE_PROBE_H
#define BLE_PROBE_H
#include "main.h"

/* Results, watch these in Live Expressions */
extern volatile uint8_t  ble_probe_rx[5];
extern volatile uint8_t  ble_probe_irq;     /* IRQ pin level after reset */
extern volatile uint8_t  ble_probe_ok;      /* 1 = module answered 0x02  */
extern volatile uint32_t ble_probe_spi_status;
void BLE_Probe(void);
#endif

