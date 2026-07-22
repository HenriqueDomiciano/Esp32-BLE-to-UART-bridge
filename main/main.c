#include "esp_bt.h"
#include "freertos/idf_additions.h"
#include "hal/uart_types.h"
#include "host/ble_gatt.h"
#include "host/ble_hs.h"
#include "include.h"
#include "soc/gpio_num.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>


static BleState_e led_state = DISCONNECTED;

static UartConnection Port0 = {0};
static UartConnection Port1 = {0};

#if CONFIG_IDF_TARGET_ESP32S3
static UartConnection Port2 = {0};
#endif

void ble_store_config_init(void);

static void ble_spp_server_host_task(void *param) {
  MODLOG_DFLT(INFO, "BLE Host Task Started");
  nimble_port_run();

  nimble_port_freertos_deinit();
}


void app_main(void) {
  int rc;
  xTaskCreate(led_task, "uTaskBlink", 2048, (void *)&led_state, 2, NULL);
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  Port0.Uart_port = UART_NUM_0;
#if CONFIG_IDF_TARGET_ESP32S3
  Port0.tx = GPIO_NUM_6;
  Port0.rx = GPIO_NUM_7;
#else
  Port0.tx = GPIO_NUM_9;
  Port0.rx = GPIO_NUM_10;
#endif
  Port1.Uart_port = UART_NUM_1;
  Port1.tx = GPIO_NUM_4;
  Port1.rx = GPIO_NUM_5;

#if CONFIG_IDF_TARGET_ESP32S3
  Port2.Uart_port = UART_NUM_2;
  Port2.tx = GPIO_NUM_1;
  Port2.rx = GPIO_NUM_2;
#endif

  ret = nimble_port_init();
  if (ret != ESP_OK) {
    MODLOG_DFLT(ERROR, "Failed to init nimble %d \n", ret);
    return;
  }

  ble_spp_uart_init(&Port0);
  ble_spp_uart_init(&Port1);
#if CONFIG_IDF_TARGET_ESP32S3
  uart2_notify = false;
  ble_spp_uart_init(&Port2);
#endif
  ble_hs_cfg.reset_cb = ble_spp_server_on_reset;
  ble_hs_cfg.sync_cb = ble_spp_server_on_sync;
  ble_hs_cfg.gatts_register_cb = gatt_svr_register_cb;
  ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

  ble_hs_cfg.sm_io_cap = BLE_HS_IO_NO_INPUT_OUTPUT;
  rc = gatt_svr_init();
  assert(rc == 0);
  rc = ble_svc_gap_device_name_set(BLE_ADVERTISING_NAME);
  assert(rc == 0);
  ble_store_config_init();
  nimble_port_freertos_init(ble_spp_server_host_task);
  ble_att_set_preferred_mtu(BLE_PREFERRED_MTU);
  esp_power_level_t default_power =
      get_esp32_power_value_based_on_int(CONFIG_BLE_TX_POWER_ADV);
  esp_power_level_t advertise_power =
      get_esp32_power_value_based_on_int(CONFIG_BLE_TX_POWER_DEFAULT);
  esp_ble_tx_power_set(advertise_power, BLE_DEFAULT_POWER);
  esp_ble_tx_power_set(default_power, BLE_ADVERTISING_POWER);
}
