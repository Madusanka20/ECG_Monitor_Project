#ifndef BLE_ECG_H
#define BLE_ECG_H

#include "main.h"

/* Set to 1 only after X-CUBE-BLE2 is added to the project */
#define BLE_ECG_ENABLED  0

#if BLE_ECG_ENABLED
void BLE_ECG_Init(void);
void BLE_ECG_PushSample(int32_t a, int32_t b, int32_t c);
void BLE_ECG_Process(void);
#else
static inline void BLE_ECG_Init(void) {}
static inline void BLE_ECG_PushSample(int32_t a, int32_t b, int32_t c)
{
    (void)a; (void)b; (void)c;
}
static inline void BLE_ECG_Process(void) {}
#endif

#endif /* BLE_ECG_H */
