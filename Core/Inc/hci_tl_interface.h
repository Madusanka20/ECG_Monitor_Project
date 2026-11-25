#ifndef HCI_TL_INTERFACE_H
#define HCI_TL_INTERFACE_H
#include <stdint.h>
void hci_tl_lowlevel_init(void);
void hci_tl_lowlevel_isr(void);
void BLE_TransportPoll(void);
#endif
