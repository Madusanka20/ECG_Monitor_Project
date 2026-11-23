#include "ble_ecg.h"

#if BLE_ECG_ENABLED

#include <string.h>
#include "hci.h"
#include "hci_tl.h"
#include "bluenrg1_aci.h"
#include "bluenrg1_hci_le.h"
#include "bluenrg1_events.h"

/* ---- Ring Buffer & Packet Configuration ---- */
#define BLE_RING_SIZE           512U   /* Must be power of 2 */
#define SAMPLES_PER_PACKET      10U    /* 10 samples * 12 bytes + 4-byte header = 124 bytes */
#define PACKET_HEADER_BYTES     4U
#define SAMPLE_BYTES            12U    /* 3 x int32_t */
#define ECG_PACKET_SIZE         (PACKET_HEADER_BYTES + (SAMPLES_PER_PACKET * SAMPLE_BYTES))

/* ---- Custom 128-bit UUIDs (Little-Endian byte order for BlueNRG ACI) ----
 * Service UUID:        12340000-1234-5678-9abc-def012345678
 * ECG Stream Char:     12340001-1234-5678-9abc-def012345678
 */
static const uint8_t ECG_SERVICE_UUID[16] = {
    0x78,0x56,0x34,0x12, 0xf0,0xde, 0xbc,0x9a,
    0x78,0x56, 0x34,0x12, 0x00,0x00, 0x34,0x12
};

static const uint8_t ECG_CHAR_UUID[16] = {
    0x78,0x56,0x34,0x12, 0xf0,0xde, 0xbc,0x9a,
    0x78,0x56, 0x34,0x12, 0x01,0x00, 0x34,0x12
};

typedef struct __attribute__((packed))
{
    int32_t lead_I;
    int32_t lead_II;
    int32_t v1;
} ble_ecg_sample_t;

typedef struct __attribute__((packed))
{
    uint16_t         seq;
    uint8_t          count;
    uint8_t          reserved;
    ble_ecg_sample_t samples[SAMPLES_PER_PACKET];
} ble_ecg_packet_t;

/* Ring buffer state */
static ble_ecg_sample_t ring_buf[BLE_RING_SIZE];
static volatile uint32_t ring_head = 0;
static volatile uint32_t ring_tail = 0;

/* BLE stack & GATT handles */
static uint16_t service_handle   = 0;
static uint16_t ecg_char_handle  = 0;
static volatile uint16_t conn_handle     = 0xFFFF;
static volatile uint8_t  notify_enabled  = 0;
static volatile uint8_t  adv_active      = 0;

/* Packetizer & flow control */
static ble_ecg_packet_t tx_pkt;
static uint8_t          tx_pkt_ready = 0;
static uint16_t         pkt_seq      = 0;

/* Debug counters (watch in STM32CubeIDE Live Expressions) */
volatile uint32_t   ble_pkts_sent    = 0;
volatile uint32_t   ble_pkts_dropped = 0;
volatile uint32_t   ble_tx_retries   = 0;
volatile uint8_t    ble_init_step    = 0;
volatile tBleStatus ble_init_err     = 0;

static void Start_Advertising(void);

uint8_t BLE_ECG_IsConnected(void)
{
    return (conn_handle != 0xFFFF) ? 1U : 0U;
}

void BLE_ECG_PushSample(int32_t lead_I, int32_t lead_II, int32_t v1)
{
    if (!notify_enabled)
    {
        return;
    }

    uint32_t next_head = (ring_head + 1U) & (BLE_RING_SIZE - 1U);
    if (next_head == ring_tail)
    {
        /* Ring buffer full: drop oldest sample to keep real-time stream fresh */
        ring_tail = (ring_tail + 1U) & (BLE_RING_SIZE - 1U);
        ble_pkts_dropped++;
    }

    ring_buf[ring_head].lead_I  = lead_I;
    ring_buf[ring_head].lead_II = lead_II;
    ring_buf[ring_head].v1      = v1;
    ring_head = next_head;
}

static void Start_Advertising(void)
{
    const char local_name[] = {
        AD_TYPE_COMPLETE_LOCAL_NAME,
        'S','T','M','3','2','-','E','C','G'
    };

    hci_le_set_scan_response_data(0, NULL);

    ble_init_step = 7;
    tBleStatus ret = aci_gap_set_discoverable(
        ADV_IND,
        0x0020, 0x0040,           /* 20 ms - 40 ms advertising interval */
        STATIC_RANDOM_ADDR,       /* Works even if OTP public MAC is unprogrammed */
        NO_WHITE_LIST_USE,
        sizeof(local_name), (uint8_t *)local_name,
        0, NULL,
        0x0006, 0x000C            /* Preferred conn interval: 7.5 ms - 15 ms */
    );

    ble_init_err = ret;
    if (ret == BLE_STATUS_SUCCESS)
    {
        ble_init_step = 8;
        adv_active = 1U;
    }
}

void BLE_ECG_Init(void)
{
    /* Top 2 MSBs must be '11' (0xC0..0xFF) for a valid BLE Static Random Address */
    uint8_t bdaddr[6] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xDE};
    uint16_t gap_service_handle, gap_dev_name_handle, gap_appearance_handle;
    Service_UUID_t srv_uuid;
    Char_UUID_t chr_uuid;
    tBleStatus ret;

    ble_init_step = 1;
    hci_init(NULL, NULL);
    hci_reset();

    /* Drain the BlueNRG boot event (EVT_BLUE_INITIALIZED) before sending ACI commands */
    uint32_t t0 = HAL_GetTick();
    while ((HAL_GetTick() - t0) < 150U)
    {
        hci_user_evt_proc();
    }

    ble_init_step = 2;
    ret = hci_le_set_random_address(bdaddr);
    if (ret != BLE_STATUS_SUCCESS) { ble_init_err = ret; return; }
    /* TX power: +4 dBm */
    aci_hal_set_tx_power_level(1, 0x06);

    ble_init_step = 3;
    ret = aci_gatt_init();
    if (ret != BLE_STATUS_SUCCESS) { ble_init_err = ret; return; }

    ble_init_step = 4;
    ret = aci_gap_init(GAP_PERIPHERAL_ROLE, 0x00, 0x09,
                       &gap_service_handle, &gap_dev_name_handle, &gap_appearance_handle);
    if (ret != BLE_STATUS_SUCCESS) { ble_init_err = ret; return; }

    /* Set GAP Device Name characteristic to match "STM32-ECG" */
    aci_gatt_update_char_value(gap_service_handle, gap_dev_name_handle,
                               0, 9, (uint8_t *)"STM32-ECG");

    /* Add Custom ECG Service */
    ble_init_step = 5;
    memcpy(srv_uuid.Service_UUID_128, ECG_SERVICE_UUID, 16);
    ret = aci_gatt_add_service(UUID_TYPE_128, &srv_uuid, PRIMARY_SERVICE, 6, &service_handle);
    if (ret != BLE_STATUS_SUCCESS) { ble_init_err = ret; return; }

    /* Add ECG Stream Characteristic (Notify, up to 124 bytes) */
    ble_init_step = 6;
    memcpy(chr_uuid.Char_UUID_128, ECG_CHAR_UUID, 16);
    ret = aci_gatt_add_char(service_handle,
                            UUID_TYPE_128,
                            &chr_uuid,
                            ECG_PACKET_SIZE,
                            CHAR_PROP_NOTIFY,
                            ATTR_PERMISSION_NONE,
                            GATT_DONT_NOTIFY_EVENTS,
                            16,
                            1,
                            &ecg_char_handle);
    if (ret != BLE_STATUS_SUCCESS) { ble_init_err = ret; return; }

    Start_Advertising();
}

void BLE_ECG_Process(void)
{
    hci_user_evt_proc();

    if (conn_handle == 0xFFFF || !notify_enabled)
    {
        return;
    }

    if (!tx_pkt_ready)
    {
        uint32_t available = (ring_head - ring_tail) & (BLE_RING_SIZE - 1U);
        if (available < SAMPLES_PER_PACKET)
        {
            return;
        }

        tx_pkt.seq      = pkt_seq++;
        tx_pkt.count    = SAMPLES_PER_PACKET;
        tx_pkt.reserved = 0;

        for (uint32_t i = 0; i < SAMPLES_PER_PACKET; i++)
        {
            tx_pkt.samples[i] = ring_buf[ring_tail];
            ring_tail = (ring_tail + 1U) & (BLE_RING_SIZE - 1U);
        }

        tx_pkt_ready = 1U;
    }

    if (tx_pkt_ready)
    {
        tBleStatus ret = aci_gatt_update_char_value(
            service_handle,
            ecg_char_handle,
            0,
            ECG_PACKET_SIZE,
            (uint8_t *)&tx_pkt
        );

        if (ret == BLE_STATUS_SUCCESS)
        {
            tx_pkt_ready = 0U;
            ble_pkts_sent++;
        }
        else if (ret == BLE_STATUS_INSUFFICIENT_RESOURCES)
        {
            ble_tx_retries++;
        }
        else
        {
            tx_pkt_ready = 0U;
            ble_pkts_dropped++;
        }
    }
}

void hci_le_connection_complete_event(uint8_t Status,
                                      uint16_t Connection_Handle,
                                      uint8_t Role,
                                      uint8_t Peer_Address_Type,
                                      uint8_t Peer_Address[6],
                                      uint16_t Conn_Interval,
                                      uint16_t Conn_Latency,
                                      uint16_t Supervision_Timeout,
                                      uint8_t Master_Clock_Accuracy)
{
    (void)Role; (void)Peer_Address_Type; (void)Peer_Address;
    (void)Conn_Interval; (void)Conn_Latency; (void)Supervision_Timeout;
    (void)Master_Clock_Accuracy;

    if (Status == 0x00)
    {
        conn_handle = Connection_Handle;
        adv_active  = 0U;

        aci_gatt_exchange_config(conn_handle);
        aci_l2cap_connection_parameter_update_req(conn_handle, 0x0006, 0x000C, 0, 0x00C8);
    }
}

void hci_disconnection_complete_event(uint8_t Status,
                                         uint16_t Connection_Handle,
                                         uint8_t Reason)
{
    (void)Status; (void)Connection_Handle; (void)Reason;

    conn_handle    = 0xFFFF;
    notify_enabled = 0U;
    tx_pkt_ready   = 0U;
    ring_head      = 0U;
    ring_tail      = 0U;

    Start_Advertising();
}

void aci_gatt_attribute_modified_event(uint16_t Connection_Handle,
                                       uint16_t Attr_Handle,
                                       uint16_t Offset,
                                       uint16_t Attr_Data_Length,
                                       uint8_t Attr_Data[])
{
    (void)Connection_Handle; (void)Offset;

    if ((Attr_Handle == (ecg_char_handle + 2U)) && (Attr_Data_Length == 2U))
    {
        if (Attr_Data[0] & 0x01U)
        {
            ring_head      = 0U;
            ring_tail      = 0U;
            tx_pkt_ready   = 0U;
            pkt_seq        = 0U;
            notify_enabled = 1U;
        }
        else
        {
            notify_enabled = 0U;
        }
    }
}

#endif /* BLE_ECG_ENABLED */
