#include "ble_probe.h"
#include "bluenrg1_hci_le.h"

volatile uint8_t ble_probe_ok, ble_probe_status;
volatile uint16_t ble_hci_revision, ble_manufacturer;

void BLE_Probe(void)
{
    uint8_t hci_version = 0, ll_version = 0;
    uint16_t revision = 0, manufacturer = 0, subversion = 0;
    ble_probe_status = hci_read_local_version_information(&hci_version, &revision,
                                      &ll_version, &manufacturer, &subversion);
    ble_probe_ok = (ble_probe_status == 0);
    ble_hci_revision = revision;
    ble_manufacturer = manufacturer;
}
