#ifndef BLE_ECG_H
#define BLE_ECG_H
#include <stdint.h>
#define BLE_ECG_ENABLED 1
void BLE_ECG_Init(void);
void BLE_ECG_Process(void);
void BLE_ECG_PushSample(uint32_t index, int32_t lead_i, int32_t lead_ii, int32_t v1);
void BLE_ECG_MarkGap(void);
uint8_t BLE_ECG_IsConnected(void);
#endif
