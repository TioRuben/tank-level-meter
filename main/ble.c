#include "ble.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "esp_err.h"
#include "esp_log.h"
#include "sensor.h"

#define BLE_DEVICE_NAME "ECHH4_R_Tank"
#define BLE_TASK_STACK_SIZE 4096
#define BLE_TASK_PRIORITY 5
#define BLE_NOTIFY_PERIOD_MS 1000
#define BLE_COMMAND_QUEUE_LENGTH 8

#define BLE_COMMAND_PING 0x00
#define BLE_COMMAND_START_CALIBRATION 0x01
#define BLE_COMMAND_COMMIT_EMPTY 0x02
#define BLE_COMMAND_COMMIT_ZERO 0x03
#define BLE_COMMAND_COMMIT_FULL 0x04
#define BLE_COMMAND_CANCEL_CALIBRATION 0x05
#define BLE_COMMAND_SAMPLE 0x06

static const char *TAG = "ble";
static QueueHandle_t command_queue;
static uint16_t connection_handle = BLE_HS_CONN_HANDLE_NONE;
static uint16_t measurement_value_handle;
static uint16_t status_value_handle;
static uint16_t info_value_handle;
static uint8_t ble_address_type;
static uint8_t last_command_status;

static const ble_uuid128_t service_uuid =
    BLE_UUID128_INIT(0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80,
                     0x00, 0x10, 0x00, 0x00, 0x00, 0xa1, 0x00, 0x00);
static const ble_uuid128_t measurement_uuid =
    BLE_UUID128_INIT(0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80,
                     0x00, 0x10, 0x00, 0x00, 0x00, 0xa1, 0x01, 0x00);
static const ble_uuid128_t control_uuid =
    BLE_UUID128_INIT(0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80,
                     0x00, 0x10, 0x00, 0x00, 0x00, 0xa1, 0x02, 0x00);
static const ble_uuid128_t status_uuid =
    BLE_UUID128_INIT(0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80,
                     0x00, 0x10, 0x00, 0x00, 0x00, 0xa1, 0x03, 0x00);
static const ble_uuid128_t info_uuid =
    BLE_UUID128_INIT(0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80,
                     0x00, 0x10, 0x00, 0x00, 0x00, 0xa1, 0x04, 0x00);

static void ble_start_advertising(void);

static int ble_append_snapshot(struct os_mbuf *om,
                               const sensor_snapshot_t *snapshot)
{
    uint8_t payload[9] = {
        1,
        snapshot->level,
        snapshot->raw[1],
        snapshot->raw[2],
        snapshot->raw[3],
        snapshot->last_error == ESP_OK ? 0x01 : 0x02,
        (uint8_t)sensor_calibration_get_state(),
        (uint8_t)snapshot->sample_count,
        (uint8_t)(snapshot->sample_count >> 8),
    };
    return os_mbuf_append(om, payload, sizeof(payload));
}

static int ble_append_status(struct os_mbuf *om)
{
    uint8_t payload[2] = {
        (uint8_t)sensor_calibration_get_state(),
        last_command_status,
    };
    return os_mbuf_append(om, payload, sizeof(payload));
}

static void ble_notify(void)
{
    if (connection_handle == BLE_HS_CONN_HANDLE_NONE)
    {
        return;
    }

    sensor_snapshot_t snapshot;
    if (sensor_get_latest(&snapshot) != ESP_OK)
    {
        return;
    }

    struct os_mbuf *measurement = ble_hs_mbuf_from_flat(NULL, 0);
    if (measurement != NULL && ble_append_snapshot(measurement, &snapshot) == 0)
    {
        ble_gatts_notify_custom(connection_handle,
                                measurement_value_handle,
                                measurement);
    }

    struct os_mbuf *status = ble_hs_mbuf_from_flat(NULL, 0);
    if (status != NULL && ble_append_status(status) == 0)
    {
        ble_gatts_notify_custom(connection_handle, status_value_handle, status);
    }
}

static void ble_worker_task(void *arg)
{
    uint8_t command;
    while (true)
    {
        if (xQueueReceive(command_queue, &command, portMAX_DELAY) != pdTRUE)
        {
            continue;
        }

        esp_err_t err = ESP_OK;
        switch (command)
        {
        case BLE_COMMAND_PING:
        case BLE_COMMAND_SAMPLE:
            break;
        case BLE_COMMAND_START_CALIBRATION:
        case BLE_COMMAND_COMMIT_EMPTY:
        case BLE_COMMAND_COMMIT_ZERO:
        case BLE_COMMAND_COMMIT_FULL:
        case BLE_COMMAND_CANCEL_CALIBRATION:
            err = ESP_ERR_NOT_SUPPORTED;
            break;
        default:
            err = ESP_ERR_INVALID_ARG;
            break;
        }

        last_command_status = err == ESP_OK ? 0 : (uint8_t)(err & 0xff);
        ESP_LOGI(TAG, "Command 0x%02X completed: %s", command,
                 esp_err_to_name(err));
        ble_notify();
    }
}

static void ble_notify_task(void *arg)
{
    while (true)
    {
        ble_notify();
        vTaskDelay(pdMS_TO_TICKS(BLE_NOTIFY_PERIOD_MS));
    }
}

static int ble_control_access(uint16_t connection, uint16_t attribute,
                              struct ble_gatt_access_ctxt *context, void *arg)
{
    if (context->op != BLE_GATT_ACCESS_OP_WRITE_CHR ||
        OS_MBUF_PKTLEN(context->om) != 1)
    {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }

    uint8_t command;
    if (os_mbuf_copydata(context->om, 0, sizeof(command), &command) != 0 ||
        xQueueSend(command_queue, &command, 0) != pdTRUE)
    {
        return BLE_ATT_ERR_INSUFFICIENT_RES;
    }
    return 0;
}

static int ble_measurement_access(uint16_t connection, uint16_t attribute,
                                  struct ble_gatt_access_ctxt *context,
                                  void *arg)
{
    sensor_snapshot_t snapshot;
    if (context->op != BLE_GATT_ACCESS_OP_READ_CHR ||
        sensor_get_latest(&snapshot) != ESP_OK)
    {
        return BLE_ATT_ERR_UNLIKELY;
    }
    return ble_append_snapshot(context->om, &snapshot) == 0
               ? 0
               : BLE_ATT_ERR_INSUFFICIENT_RES;
}

static int ble_status_access(uint16_t connection, uint16_t attribute,
                             struct ble_gatt_access_ctxt *context, void *arg)
{
    if (context->op != BLE_GATT_ACCESS_OP_READ_CHR)
    {
        return BLE_ATT_ERR_READ_NOT_PERMITTED;
    }
    return ble_append_status(context->om) == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

static int ble_info_access(uint16_t connection, uint16_t attribute,
                           struct ble_gatt_access_ctxt *context, void *arg)
{
    static const char info[] = "tank_level_sensor phase3";
    if (context->op != BLE_GATT_ACCESS_OP_READ_CHR)
    {
        return BLE_ATT_ERR_READ_NOT_PERMITTED;
    }
    return os_mbuf_append(context->om, info, sizeof(info) - 1) == 0
               ? 0
               : BLE_ATT_ERR_INSUFFICIENT_RES;
}

static const struct ble_gatt_svc_def gatt_services[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &service_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]){
            {
                .uuid = &measurement_uuid.u,
                .access_cb = ble_measurement_access,
                .val_handle = &measurement_value_handle,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
            },
            {
                .uuid = &control_uuid.u,
                .access_cb = ble_control_access,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
            },
            {
                .uuid = &status_uuid.u,
                .access_cb = ble_status_access,
                .val_handle = &status_value_handle,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
            },
            {
                .uuid = &info_uuid.u,
                .access_cb = ble_info_access,
                .val_handle = &info_value_handle,
                .flags = BLE_GATT_CHR_F_READ,
            },
            {0},
        },
    },
    {0},
};

static int ble_gap_event(struct ble_gap_event *event, void *arg)
{
    switch (event->type)
    {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0)
        {
            if (connection_handle != BLE_HS_CONN_HANDLE_NONE)
            {
                ble_gap_terminate(event->connect.conn_handle,
                                  BLE_ERR_REM_USER_CONN_TERM);
            }
            else
            {
                connection_handle = event->connect.conn_handle;
                ESP_LOGI(TAG, "BLE central connected");
            }
        }
        else
        {
            ble_start_advertising();
        }
        return 0;
    case BLE_GAP_EVENT_DISCONNECT:
        connection_handle = BLE_HS_CONN_HANDLE_NONE;
        ble_start_advertising();
        ESP_LOGI(TAG, "BLE central disconnected");
        return 0;
    case BLE_GAP_EVENT_ADV_COMPLETE:
        ble_start_advertising();
        return 0;
    default:
        return 0;
    }
}

static void ble_start_advertising(void)
{
    struct ble_hs_adv_fields fields = {0};
    fields.uuids128 = (ble_uuid128_t *)&service_uuid;
    fields.num_uuids128 = 1;
    fields.uuids128_is_complete = 1;
    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "Could not set BLE advertisement fields: rc=%d", rc);
        return;
    }

    struct ble_hs_adv_fields response_fields = {0};
    response_fields.name = (uint8_t *)BLE_DEVICE_NAME;
    response_fields.name_len = strlen(BLE_DEVICE_NAME);
    response_fields.name_is_complete = 1;
    rc = ble_gap_adv_rsp_set_fields(&response_fields);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "Could not set BLE scan response fields: rc=%d", rc);
        return;
    }

    struct ble_gap_adv_params params = {0};
    params.conn_mode = BLE_GAP_CONN_MODE_UND;
    params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    rc = ble_gap_adv_start(ble_address_type, NULL, BLE_HS_FOREVER, &params,
                           ble_gap_event, NULL);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "Could not start BLE advertising: rc=%d", rc);
    }
}

static void ble_on_sync(void)
{
    ble_hs_id_infer_auto(0, &ble_address_type);
    ble_start_advertising();
}

static void ble_host_task(void *arg)
{
    nimble_port_run();
    nimble_port_freertos_deinit();
}

esp_err_t app_ble_start(void)
{
    command_queue = xQueueCreate(BLE_COMMAND_QUEUE_LENGTH, sizeof(uint8_t));
    if (command_queue == NULL)
    {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = nimble_port_init();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "NimBLE/controller initialization failed: %s",
                 esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "Bluetooth controller and NimBLE host initialized");
    ble_svc_gap_init();
    ble_svc_gatt_init();
    ble_svc_gap_device_name_set(BLE_DEVICE_NAME);
    ble_gatts_count_cfg(gatt_services);
    ble_gatts_add_svcs(gatt_services);
    ble_hs_cfg.sync_cb = ble_on_sync;

    xTaskCreate(ble_worker_task, "ble_worker", BLE_TASK_STACK_SIZE, NULL,
                BLE_TASK_PRIORITY, NULL);
    xTaskCreate(ble_notify_task, "ble_notify", BLE_TASK_STACK_SIZE, NULL,
                BLE_TASK_PRIORITY, NULL);
    nimble_port_freertos_init(ble_host_task);
    return ESP_OK;
}