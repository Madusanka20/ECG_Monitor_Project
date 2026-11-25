#include "main.h"
#include "ble_ecg.h"
#include "ble_probe.h"
#include "hci.h"
#include "hci_tl.h"
#include "bluenrg1_aci.h"
#include "bluenrg1_hci_le.h"
#include <string.h>

#define QUEUE_SIZE 512U
#define QUEUE_MASK (QUEUE_SIZE - 1U)
#define DISCONNECTED 0xFFFFU

static const uint8_t service_uuid[16] = {
    0x78,0x56,0x34,0x12,0xF0,0xDE,0xBC,0x9A,0x78,0x56,0x34,0x12,0,0,0x34,0x12
};
typedef struct {
    uint32_t index;
    int32_t channels[3];
} sample_t;

static sample_t queue[QUEUE_SIZE];
static volatile uint32_t head, tail;
static uint16_t service, characteristic;
static uint8_t stack_ready, advertise_pending, interval_pending;
static uint32_t last_adv_attempt, tx_attempt;
static uint8_t tx_delayed;
static volatile uint32_t index_offset;
volatile uint16_t ble_connection = DISCONNECTED;
volatile uint8_t ble_subscribed, ble_init_step, ble_init_err, ble_last_tx_status;
volatile uint32_t ble_pkts_sent, ble_queue_drops, ble_tx_retries, ble_tx_drops;

static uint16_t u16(const uint8_t *p)
{
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

static void set_subscription(uint8_t enabled)
{
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    tail = head;
    ble_subscribed = enabled;
    __set_PRIMASK(mask);
    tx_delayed = 0;
}

static void on_event(void *data)
{
    const uint8_t *p = data;
    if (!p || p[0] != 0x04) return;
    uint8_t event = p[1], length = p[2];
    const uint8_t *v = p + 3;
    if (event == 0x3E && length >= 4 && (v[0] == 1 || v[0] == 10) && v[1] == 0)
    {
        ble_connection = u16(v + 2);
        set_subscription(0);
        interval_pending = 1;
        advertise_pending = 0;
    }
    else if (event == 0x05 && length >= 4 && v[0] == 0 && u16(v+1) == ble_connection)
    {
        ble_connection = DISCONNECTED;
        set_subscription(0);
        interval_pending = 0;
        advertise_pending = 1;
    }
    else if (event == 0xFF && length >= 12 && u16(v) == 0x0C01 &&
             ble_connection != DISCONNECTED && u16(v+2) == ble_connection &&
             u16(v+4) == characteristic + 2U && u16(v+6) == 0 && u16(v+8) == 2)
    {
        set_subscription((uint8_t)(u16(v+10) & 1U));
    }
}

uint8_t BLE_ECG_IsConnected(void)
{
    return ble_connection != DISCONNECTED;
}

void BLE_ECG_PushSample(uint32_t index, int32_t i, int32_t ii, int32_t v)
{
    if (!ble_subscribed) return;
    uint32_t next = (head + 1U) & QUEUE_MASK;
    if (next == tail)
    {
        ble_queue_drops++;
        return;
    }
    queue[head] = (sample_t){index + index_offset, {i, ii, v}};
    __DMB();
    head = next;
}

void BLE_ECG_MarkGap(void)
{
    index_offset++;
}

static uint8_t advertise(void)
{
    uint8_t name[] = {AD_TYPE_COMPLETE_LOCAL_NAME,'S','T','M','3','2','-','E','C','G'};
    last_adv_attempt = HAL_GetTick();
    return aci_gap_set_discoverable(ADV_IND, 0x30, 0x60, STATIC_RANDOM_ADDR,
                                    NO_WHITE_LIST_USE, sizeof(name), name, 0, NULL, 6, 12);
}

void BLE_ECG_Init(void)
{
    uint16_t gap, name, appearance;
    Service_UUID_t su;
    Char_UUID_t cu;
    memcpy(su.Service_UUID_128, service_uuid, 16);
    memcpy(cu.Char_UUID_128, service_uuid, 16);
    cu.Char_UUID_128[12] = 1;

    ble_init_step = 1;
    hci_init(on_event, NULL);
    ble_init_err = hci_reset();
    if (ble_init_err) return;
    HAL_Delay(2000);
    BLE_TransportPoll();
    hci_user_evt_proc();

    BLE_Probe();
    if (!ble_probe_ok) { ble_init_err = ble_probe_status; return; }
    ble_init_step = 2;
    ble_init_err = aci_gatt_init();
    if (ble_init_err) return;
    ble_init_step = 3;
    ble_init_err = aci_gap_init(GAP_PERIPHERAL_ROLE, 0, 9, &gap, &name, &appearance);
    if (ble_init_err) return;
    ble_init_step = 4;
    uint8_t device_name[] = "STM32-ECG";
    ble_init_err = aci_gatt_update_char_value(gap, name, 0, sizeof(device_name)-1, device_name);
    if (ble_init_err) return;
    ble_init_step = 5;
    ble_init_err = aci_gatt_add_service(UUID_TYPE_128, &su, PRIMARY_SERVICE, 4, &service);
    if (ble_init_err) return;
    ble_init_step = 6;
    ble_init_err = aci_gatt_add_char(service, UUID_TYPE_128, &cu, 20, CHAR_PROP_NOTIFY,
                    ATTR_PERMISSION_NONE, GATT_NOTIFY_ATTRIBUTE_WRITE, 16, 1, &characteristic);
    if (ble_init_err) return;
    ble_init_step = 7;
    uint8_t empty_scan[31] = {0};
    ble_init_err = hci_le_set_scan_response_data(0, empty_scan);
    if (ble_init_err) return;
    stack_ready = 1;
    ble_init_err = advertise();
    advertise_pending = ble_init_err != 0;
    if (!advertise_pending) ble_init_step = 8;
}

static void pack_sample(uint8_t *dst, const sample_t *s)
{
    for (unsigned c = 0; c < 3; ++c)
    {
        uint32_t value = (uint32_t)s->channels[c];
        dst[3*c] = (uint8_t)value;
        dst[3*c+1] = (uint8_t)(value >> 8);
        dst[3*c+2] = (uint8_t)(value >> 16);
    }
}

void BLE_ECG_Process(void)
{
    if (!stack_ready) return;
    BLE_TransportPoll();
    hci_user_evt_proc();
    if (advertise_pending && HAL_GetTick() - last_adv_attempt >= 250)
    {
        ble_init_err = advertise();
        advertise_pending = ble_init_err != 0;
        if (!advertise_pending) ble_init_step = 8;
    }
    if (interval_pending)
    {
        interval_pending = 0;
        aci_l2cap_connection_parameter_update_req(ble_connection, 6, 12, 0, 200);
    }
    if (!ble_subscribed || (tx_delayed && HAL_GetTick() - tx_attempt < 3)) return;
    uint32_t first = tail, second = (first + 1U) & QUEUE_MASK;
    if (first == head || second == head) return;

    const sample_t *a = &queue[first], *b = &queue[second];
    uint8_t payload[20] = {(uint8_t)a->index, (uint8_t)(a->index >> 8)};
    unsigned count = 1;
    pack_sample(payload + 2, a);
    if (b->index == a->index + 1U)
    {
        pack_sample(payload + 11, b);
        count = 2;
    }
    ble_last_tx_status = aci_gatt_update_char_value(service, characteristic, 0,
                                                    2U + 9U*count, payload);
    if (ble_last_tx_status == BLE_STATUS_INSUFFICIENT_RESOURCES)
    {
        ble_tx_retries++;
        tx_attempt = HAL_GetTick();
        tx_delayed = 1;
        return;
    }
    tx_delayed = 0;
    if (ble_last_tx_status == BLE_STATUS_SUCCESS) ble_pkts_sent++;
    else ble_tx_drops += count;
    tail = (first + count) & QUEUE_MASK;
}
