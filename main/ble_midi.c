#include "ble_midi.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#define DEVICE_NAME "Pocket Looper"
#define LOG_HEX_BYTES 32
#define HIGH_BIT 0x80
#define MAX_MIDI_MESSAGE_LENGTH 3

static const char *TAG = "ble_midi";

static const ble_uuid128_t s_service_uuid = BLE_UUID128_INIT(
    0x00, 0xC7, 0xC4, 0x4E, 0xE3, 0x6C, 0x51, 0xA7,
    0x33, 0x4B, 0xE8, 0xED, 0x5A, 0x0E, 0xB8, 0x03);

static const ble_uuid128_t s_characteristic_uuid = BLE_UUID128_INIT(
    0xF3, 0x6B, 0x10, 0x9D, 0x66, 0xF2, 0xA9, 0xA1,
    0x12, 0x41, 0x68, 0x38, 0xDB, 0xE5, 0x72, 0x77);

static const struct ble_gap_upd_params s_connection_params = {
    .itvl_min = 6,
    .itvl_max = 12,
    .latency = 0,
    .supervision_timeout = 400,
};

static ble_midi_callbacks_t s_callbacks;
static uint16_t s_characteristic_handle;
static uint8_t s_own_addr_type;
static volatile uint16_t s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static volatile bool s_subscribed;

static uint8_t s_write_packet[BLE_ATT_ATTR_MAX_LEN];
static uint8_t s_write_midi[BLE_ATT_ATTR_MAX_LEN];

void ble_store_config_init(void);

static int on_gap_event(struct ble_gap_event *event, void *arg);

static void log_bytes(const char *direction, const uint8_t *data, size_t length)
{
    char hex[LOG_HEX_BYTES * 3 + 1] = "";
    size_t shown = length < LOG_HEX_BYTES ? length : LOG_HEX_BYTES;
    for (size_t i = 0; i < shown; i++) {
        snprintf(&hex[i * 3], 4, "%02X ", data[i]);
    }
    ESP_LOGI(TAG, "%s %s%s", direction, hex, shown < length ? "..." : "");
}

/* MIDI status bytes also have the high bit set, so a high-bit byte is a timestamp unless the byte before it was one. */
static size_t strip_framing(const uint8_t *packet, size_t length, uint8_t *midi)
{
    size_t out = 0;
    bool after_timestamp = false;
    for (size_t i = 1; i < length; i++) {
        uint8_t byte = packet[i];
        if ((byte & HIGH_BIT) != 0 && !after_timestamp) {
            after_timestamp = true;
            continue;
        }
        after_timestamp = false;
        midi[out++] = byte;
    }
    return out;
}

static void link_down(void)
{
    bool was_connected = s_conn_handle != BLE_HS_CONN_HANDLE_NONE;
    s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
    s_subscribed = false;
    if (was_connected) {
        s_callbacks.on_link_changed(false);
    }
}

static void advertise(void)
{
    struct ble_hs_adv_fields fields = {0};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.uuids128 = &s_service_uuid;
    fields.num_uuids128 = 1;
    fields.uuids128_is_complete = 1;
    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "Setting advertising data failed: %d", rc);
        return;
    }

    /* The 128-bit UUID leaves no room for the name in the 31-byte advertising packet. */
    struct ble_hs_adv_fields response = {0};
    response.name = (const uint8_t *)DEVICE_NAME;
    response.name_len = strlen(DEVICE_NAME);
    response.name_is_complete = 1;
    rc = ble_gap_adv_rsp_set_fields(&response);
    if (rc != 0) {
        ESP_LOGE(TAG, "Setting scan response failed: %d", rc);
        return;
    }

    struct ble_gap_adv_params params = {0};
    params.conn_mode = BLE_GAP_CONN_MODE_UND;
    params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    rc = ble_gap_adv_start(s_own_addr_type, NULL, BLE_HS_FOREVER, &params, on_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Starting advertising failed: %d", rc);
    }
}

static int on_gap_event(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status != 0) {
            ESP_LOGW(TAG, "Connection attempt failed: %d", event->connect.status);
            advertise();
            return 0;
        }
        ESP_LOGI(TAG, "Connected, handle %d", event->connect.conn_handle);
        s_conn_handle = event->connect.conn_handle;
        s_callbacks.on_link_changed(true);
        if (ble_gap_update_params(event->connect.conn_handle, &s_connection_params) != 0) {
            ESP_LOGW(TAG, "Requesting connection parameters failed");
        }
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "Disconnected, reason %d", event->disconnect.reason);
        link_down();
        advertise();
        return 0;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        advertise();
        return 0;

    case BLE_GAP_EVENT_SUBSCRIBE:
        if (event->subscribe.attr_handle == s_characteristic_handle) {
            s_subscribed = event->subscribe.cur_notify;
            ESP_LOGI(TAG, "Notifications %s", s_subscribed ? "enabled" : "disabled");
            if (s_callbacks.on_subscription_changed != NULL) {
                s_callbacks.on_subscription_changed(s_subscribed);
            }
        }
        return 0;

    case BLE_GAP_EVENT_CONN_UPDATE: {
        struct ble_gap_conn_desc desc;
        if (ble_gap_conn_find(event->conn_update.conn_handle, &desc) == 0) {
            ESP_LOGI(TAG, "Connection parameters: interval %d x 1.25 ms, latency %d, timeout %d x 10 ms",
                     desc.conn_itvl, desc.conn_latency, desc.supervision_timeout);
        }
        return 0;
    }

    case BLE_GAP_EVENT_REPEAT_PAIRING: {
        struct ble_gap_conn_desc desc;
        if (ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc) == 0) {
            ble_store_util_delete_peer(&desc.peer_id_addr);
        }
        return BLE_GAP_REPEAT_PAIRING_RETRY;
    }

    default:
        return 0;
    }
}

static int on_midi_access(uint16_t conn_handle, uint16_t attr_handle,
                          struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    switch (ctxt->op) {
    case BLE_GATT_ACCESS_OP_READ_CHR:
        return 0;

    case BLE_GATT_ACCESS_OP_WRITE_CHR: {
        uint16_t length = 0;
        if (ble_hs_mbuf_to_flat(ctxt->om, s_write_packet, sizeof(s_write_packet), &length) != 0) {
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }
        size_t midi_length = strip_framing(s_write_packet, length, s_write_midi);
        if (midi_length > 0) {
            log_bytes("RX", s_write_midi, midi_length);
            if (s_callbacks.on_receive != NULL) {
                s_callbacks.on_receive(s_write_midi, midi_length);
            }
        }
        return 0;
    }

    default:
        return BLE_ATT_ERR_UNLIKELY;
    }
}

static const struct ble_gatt_svc_def s_services[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &s_service_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid = &s_characteristic_uuid.u,
                .access_cb = on_midi_access,
                .val_handle = &s_characteristic_handle,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE_NO_RSP | BLE_GATT_CHR_F_NOTIFY,
            },
            {0},
        },
    },
    {0},
};

static void on_reset(int reason)
{
    ESP_LOGW(TAG, "Host reset, reason %d", reason);
    link_down();
}

static void on_sync(void)
{
    int rc = ble_hs_util_ensure_addr(0);
    if (rc == 0) {
        rc = ble_hs_id_infer_auto(0, &s_own_addr_type);
    }
    if (rc != 0) {
        ESP_LOGE(TAG, "Resolving the device address failed: %d", rc);
        return;
    }
    advertise();
}

static void host_task(void *param)
{
    nimble_port_run();
    nimble_port_freertos_deinit();
}

esp_err_t ble_midi_init(const ble_midi_callbacks_t *callbacks)
{
    if (callbacks == NULL || callbacks->on_link_changed == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    s_callbacks = *callbacks;

    esp_err_t err = nimble_port_init();
    if (err != ESP_OK) {
        return err;
    }

    ble_hs_cfg.reset_cb = on_reset;
    ble_hs_cfg.sync_cb = on_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    ble_hs_cfg.sm_io_cap = BLE_HS_IO_NO_INPUT_OUTPUT;
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_sc = 1;
    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;

    ble_svc_gap_init();
    ble_svc_gatt_init();
    if (ble_gatts_count_cfg(s_services) != 0 || ble_gatts_add_svcs(s_services) != 0) {
        return ESP_FAIL;
    }
    if (ble_svc_gap_device_name_set(DEVICE_NAME) != 0) {
        return ESP_FAIL;
    }
    ble_store_config_init();

    nimble_port_freertos_init(host_task);
    return ESP_OK;
}

bool ble_midi_is_connected(void)
{
    return s_conn_handle != BLE_HS_CONN_HANDLE_NONE;
}

bool ble_midi_can_send(void)
{
    return s_conn_handle != BLE_HS_CONN_HANDLE_NONE && s_subscribed;
}

static esp_err_t send_midi(const uint8_t *midi, size_t length)
{
    log_bytes("TX", midi, length);
    if (!s_subscribed) {
        ESP_LOGW(TAG, "Not sent: no central has enabled notifications");
        return ESP_ERR_INVALID_STATE;
    }

    uint32_t ms = (uint32_t)(esp_timer_get_time() / 1000);
    uint8_t packet[2 + MAX_MIDI_MESSAGE_LENGTH];
    packet[0] = HIGH_BIT | ((ms >> 7) & 0x3F);
    packet[1] = HIGH_BIT | (ms & 0x7F);
    memcpy(&packet[2], midi, length);

    struct os_mbuf *om = ble_hs_mbuf_from_flat(packet, 2 + length);
    if (om == NULL) {
        return ESP_ERR_NO_MEM;
    }
    int rc = ble_gatts_notify_custom(s_conn_handle, s_characteristic_handle, om);
    if (rc != 0) {
        ESP_LOGW(TAG, "Notification failed: %d", rc);
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t ble_midi_send(uint8_t status, uint8_t data1, uint8_t data2)
{
    const uint8_t midi[3] = {status, data1, data2};
    return send_midi(midi, sizeof(midi));
}

esp_err_t ble_midi_send_two_bytes(uint8_t status, uint8_t data1)
{
    const uint8_t midi[2] = {status, data1};
    return send_midi(midi, sizeof(midi));
}
