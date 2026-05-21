#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

#include "aircraft_telemetry.h"

LOG_MODULE_REGISTER(zephyr_ble_app, LOG_LEVEL_INF);

#define SENSOR_SERVICE_UUID_VAL \
  BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef0)

#define TEMP_CHAR_UUID_VAL \
  BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef1)

#define HEART_RATE_CHAR_UUID_VAL \
  BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef2)

#define BATTERY_CHAR_UUID_VAL \
  BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef3)

#define INTERVAL_CHAR_UUID_VAL \
  BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef4)

#define AIRCRAFT_MANUFACTURER_ID 0x59AF

static const struct bt_uuid_128 sensor_service_uuid = BT_UUID_INIT_128(SENSOR_SERVICE_UUID_VAL);
static const struct bt_uuid_128 temp_char_uuid = BT_UUID_INIT_128(TEMP_CHAR_UUID_VAL);
static const struct bt_uuid_128 heart_rate_char_uuid = BT_UUID_INIT_128(HEART_RATE_CHAR_UUID_VAL);
static const struct bt_uuid_128 battery_char_uuid = BT_UUID_INIT_128(BATTERY_CHAR_UUID_VAL);
static const struct bt_uuid_128 interval_char_uuid = BT_UUID_INIT_128(INTERVAL_CHAR_UUID_VAL);
struct sensor_state
{
  int16_t temperature_c_x100;
  uint8_t heart_rate_bpm;
  uint8_t battery_percent;
  uint8_t update_interval_s;
  uint32_t sample_count;
};

static struct sensor_state state = {
    .temperature_c_x100 = 2350,
    .heart_rate_bpm = 72,
    .battery_percent = 100,
    .update_interval_s = 1,
    .sample_count = 0,
};

static struct aircraft_telemetry_state aircraft_state;
static struct aircraft_telemetry_runtime aircraft_runtime;

static uint8_t temp_value[2];
static uint8_t aircraft_adv_payload[25];
static bool temp_notify_enabled;
static bool hr_notify_enabled;
static bool battery_notify_enabled;
static bool connected;
static struct k_work_delayable sensor_work;

static void sync_values(void)
{
  sys_put_le16((uint16_t)state.temperature_c_x100, temp_value);
}

static void sync_aircraft_payload(void)
{
  sys_put_le16(AIRCRAFT_MANUFACTURER_ID, &aircraft_adv_payload[0]);
  sys_put_le32(aircraft_state.aircraft_id, &aircraft_adv_payload[2]);
  sys_put_le32((uint32_t)aircraft_state.latitude_e7, &aircraft_adv_payload[6]);
  sys_put_le32((uint32_t)aircraft_state.longitude_e7, &aircraft_adv_payload[10]);
  sys_put_le16((uint16_t)aircraft_state.altitude_m, &aircraft_adv_payload[14]);
  sys_put_le16(aircraft_state.speed_kph_x10, &aircraft_adv_payload[16]);
  sys_put_le16(aircraft_state.heading_deg_x10, &aircraft_adv_payload[18]);
  aircraft_adv_payload[20] = aircraft_state.battery_percent;
  sys_put_le32(aircraft_state.timestamp_ms, &aircraft_adv_payload[21]);
}

static ssize_t read_temp(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                         void *buf, uint16_t len, uint16_t offset)
{
  return bt_gatt_attr_read(conn, attr, buf, len, offset, attr->user_data,
                           sizeof(temp_value));
}

static ssize_t read_u8(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                       void *buf, uint16_t len, uint16_t offset)
{
  return bt_gatt_attr_read(conn, attr, buf, len, offset, attr->user_data,
                           sizeof(uint8_t));
}

static ssize_t write_update_interval(struct bt_conn *conn,
                                     const struct bt_gatt_attr *attr,
                                     const void *buf, uint16_t len, uint16_t offset,
                                     uint8_t flags)
{
  uint8_t value;

  if (offset != 0U || len != sizeof(value))
  {
    return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
  }

  memcpy(&value, buf, sizeof(value));
  if (value == 0U || value > 10U)
  {
    return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
  }

  state.update_interval_s = value;
  LOG_INF("Sampling interval updated to %u s", state.update_interval_s);
  (void)k_work_reschedule(&sensor_work, K_SECONDS(state.update_interval_s));

  return len;
}

static void temp_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
  temp_notify_enabled = (value == BT_GATT_CCC_NOTIFY);
  LOG_INF("Temperature notifications %s", temp_notify_enabled ? "enabled" : "disabled");
}

static void heart_rate_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
  hr_notify_enabled = (value == BT_GATT_CCC_NOTIFY);
  LOG_INF("Heart rate notifications %s", hr_notify_enabled ? "enabled" : "disabled");
}

static void battery_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
  battery_notify_enabled = (value == BT_GATT_CCC_NOTIFY);
  LOG_INF("Battery notifications %s", battery_notify_enabled ? "enabled" : "disabled");
}

BT_GATT_SERVICE_DEFINE(sensor_svc,
                       BT_GATT_PRIMARY_SERVICE(&sensor_service_uuid),
                       BT_GATT_CHARACTERISTIC(&temp_char_uuid.uuid,
                                              BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                                              BT_GATT_PERM_READ,
                                              read_temp, NULL, temp_value),
                       BT_GATT_CCC(temp_ccc_cfg_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
                       BT_GATT_CHARACTERISTIC(&heart_rate_char_uuid.uuid,
                                              BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                                              BT_GATT_PERM_READ,
                                              read_u8, NULL, &state.heart_rate_bpm),
                       BT_GATT_CCC(heart_rate_ccc_cfg_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
                       BT_GATT_CHARACTERISTIC(&battery_char_uuid.uuid,
                                              BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                                              BT_GATT_PERM_READ,
                                              read_u8, NULL, &state.battery_percent),
                       BT_GATT_CCC(battery_ccc_cfg_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
                       BT_GATT_CHARACTERISTIC(&interval_char_uuid.uuid,
                                              BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
                                              BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
                                              read_u8, write_update_interval, &state.update_interval_s));

static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_MANUFACTURER_DATA, aircraft_adv_payload,
            sizeof(aircraft_adv_payload)),
};

static void update_aircraft_advertising(void)
{
  int err;

  sync_aircraft_payload();

  err = bt_le_adv_update_data(ad, ARRAY_SIZE(ad), NULL, 0);
  if (err != 0)
  {
    LOG_WRN("Aircraft advertising update failed (%d)", err);
  }
}

static void send_notifications(void)
{
  int err;

  if (!connected)
  {
    return;
  }

  if (temp_notify_enabled)
  {
    err = bt_gatt_notify(NULL, &sensor_svc.attrs[2], temp_value, sizeof(temp_value));
    if (err)
    {
      LOG_WRN("Temperature notification failed (%d)", err);
    }
  }

  if (hr_notify_enabled)
  {
    err = bt_gatt_notify(NULL, &sensor_svc.attrs[5], &state.heart_rate_bpm,
                         sizeof(state.heart_rate_bpm));
    if (err)
    {
      LOG_WRN("Heart rate notification failed (%d)", err);
    }
  }

  if (battery_notify_enabled)
  {
    err = bt_gatt_notify(NULL, &sensor_svc.attrs[8], &state.battery_percent,
                         sizeof(state.battery_percent));
    if (err)
    {
      LOG_WRN("Battery notification failed (%d)", err);
    }
  }
}

static void update_fake_sensor_data(void)
{
  state.sample_count++;

  state.temperature_c_x100 = 2350 + (int16_t)((state.sample_count % 30U) * 5U);
  state.heart_rate_bpm = 62U + (uint8_t)(state.sample_count % 28U);
  state.battery_percent = 100U - (uint8_t)(state.sample_count % 40U);

  if (state.battery_percent < 20U)
  {
    state.battery_percent = 100U;
  }

  sync_values();
}

static void update_fake_aircraft_data(void)
{
  aircraft_telemetry_update(&aircraft_state, &aircraft_runtime, k_uptime_get_32());

  LOG_INF("Aircraft %08x: lat=%d lon=%d alt=%d m roc=%d.%d m/s speed=%u.%u kph heading=%u.%u timestamp=%u ms",
          aircraft_state.aircraft_id,
          aircraft_state.latitude_e7, aircraft_state.longitude_e7,
          aircraft_state.altitude_m,
          aircraft_state.rate_of_climb_mps_x10 / 10, abs(aircraft_state.rate_of_climb_mps_x10 % 10),
          aircraft_state.speed_kph_x10 / 10, aircraft_state.speed_kph_x10 % 10,
          aircraft_state.heading_deg_x10 / 10, aircraft_state.heading_deg_x10 % 10,
          aircraft_state.timestamp_ms);
}

static void sensor_work_handler(struct k_work *work)
{
  update_fake_sensor_data();
  update_fake_aircraft_data();
  update_aircraft_advertising();
  send_notifications();
  (void)k_work_reschedule(&sensor_work, K_SECONDS(state.update_interval_s));
}

static void connected_cb(struct bt_conn *conn, uint8_t err)
{
  if (err != 0U)
  {
    LOG_WRN("Connection failed (err 0x%02x: %s)", err, bt_hci_err_to_str(err));
    return;
  }

  connected = true;
  LOG_INF("Connected: %s", bt_conn_dst_str(conn));
}

static void disconnected_cb(struct bt_conn *conn, uint8_t reason)
{
  connected = false;
  temp_notify_enabled = false;
  hr_notify_enabled = false;
  battery_notify_enabled = false;
  LOG_INF("Disconnected (reason 0x%02x: %s)", reason, bt_hci_err_to_str(reason));
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected_cb,
    .disconnected = disconnected_cb,
};

static void bt_ready(int err)
{
  if (err != 0)
  {
    LOG_ERR("Bluetooth init failed (err %d)", err);
    return;
  }

  LOG_INF("Bluetooth initialized");

  sync_aircraft_payload();
  err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad), NULL, 0);
  if (err)
  {
    LOG_ERR("Advertising failed to start (err %d)", err);
    return;
  }

  LOG_INF("Advertising started with aircraft telemetry payload");
  (void)k_work_reschedule(&sensor_work, K_SECONDS(state.update_interval_s));
}

int main(void)
{
  LOG_INF("Starting Zephyr BLE aircraft broadcaster");

  aircraft_telemetry_init(&aircraft_state, &aircraft_runtime);
  sync_values();
  sync_aircraft_payload();
  k_work_init_delayable(&sensor_work, sensor_work_handler);

  (void)bt_enable(bt_ready);

  while (1)
  {
    k_sleep(K_FOREVER);
  }

  return 0;
}