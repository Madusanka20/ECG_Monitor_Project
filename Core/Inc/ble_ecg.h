#ifndef BLE_ECG_H
#define BLE_ECG_H

#include "main.h"
#include <stdint.h>

/* Set to 1 to enable BlueNRG-M2SP BLE streaming */
#define BLE_ECG_ENABLED  1
extern volatile uint8_t ble_init_step;
#if BLE_ECG_ENABLED
void    BLE_ECG_Init(void);
void    BLE_ECG_PushSample(int32_t lead_I, int32_t lead_II, int32_t v1);
void    BLE_ECG_Process(void);
uint8_t BLE_ECG_IsConnected(void);
#else
static inline void    BLE_ECG_Init(void) {}
static inline void    BLE_ECG_PushSample(int32_t a, int32_t b, int32_t c) { (void)a; (void)b; (void)c; }
static inline void    BLE_ECG_Process(void) {}
static inline uint8_t BLE_ECG_IsConnected(void) { return 0; }
#endif

#endif /* BLE_ECG_H */
