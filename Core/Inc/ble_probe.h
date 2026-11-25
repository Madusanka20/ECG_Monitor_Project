#ifndef BLE_PROBE_H
#define BLE_PROBE_H
#include <stdint.h>
extern volatile uint8_t ble_probe_ok, ble_probe_status;
extern volatile uint16_t ble_hci_revision, ble_manufacturer;
void BLE_Probe(void);
#endif
