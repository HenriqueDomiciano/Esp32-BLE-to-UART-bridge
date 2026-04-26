#include "driver/uart.h"
#include "hal/uart_types.h"
#include "host/ble_gatt.h"
#include "host/ble_hs.h"
#include "include.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

static int ble_spp_server_gap_event(struct ble_gap_event *event, void *arg);
static uint8_t own_addr_type;
int gatt_svr_register(void);

bool uart0_notify = 0;
bool uart1_notify = 0;
static uint16_t active_conn = BLE_HS_CONN_HANDLE_NONE;

u_int16_t ble_spp_tx_handle_uart_1;
u_int16_t ble_spp_rx_handle_uart_1;
u_int16_t ble_spp_tx_handle_uart_0;
u_int16_t ble_spp_rx_handle_uart_0;

u_int16_t ble_spp_handle_uart_1_baud_rate;
u_int16_t ble_spp_handle_uart_0_baud_rate;

static BleState_e led_state = DISCONNECTED;

static UartConnection Port0 = {0};
static UartConnection Port1 = {0};

void ble_store_config_init(void);

/**
 * Logs information about a connection to the console.
 */
static void ble_spp_server_print_conn_desc(struct ble_gap_conn_desc *desc) {
  MODLOG_DFLT(INFO,
              "handle=%d our_ota_addr_type=%d our_ota_addr=", desc->conn_handle,
              desc->our_ota_addr.type);
  print_addr(desc->our_ota_addr.val);
  MODLOG_DFLT(INFO,
              " our_id_addr_type=%d our_id_addr=", desc->our_id_addr.type);
  print_addr(desc->our_id_addr.val);
  MODLOG_DFLT(
      INFO, " peer_ota_addr_type=%d peer_ota_addr=", desc->peer_ota_addr.type);
  print_addr(desc->peer_ota_addr.val);
  MODLOG_DFLT(INFO,
              " peer_id_addr_type=%d peer_id_addr=", desc->peer_id_addr.type);
  print_addr(desc->peer_id_addr.val);
  MODLOG_DFLT(INFO,
              " conn_itvl=%d conn_latency=%d supervision_timeout=%d "
              "encrypted=%d authenticated=%d bonded=%d\n",
              desc->conn_itvl, desc->conn_latency, desc->supervision_timeout,
              desc->sec_state.encrypted, desc->sec_state.authenticated,
              desc->sec_state.bonded);
}

/**
 * Enables advertising with the following parameters:
 *     o General discoverable mode.
 *     o Undirected connectable mode.
 */
static void ble_spp_server_advertise(void) {
  struct ble_gap_adv_params adv_params;
  struct ble_hs_adv_fields fields;
  const char *name;
  int rc;

  /**
   *  Set the advertisement data included in our advertisements:
   *     o Flags (indicates advertisement type and other general info).
   *     o Advertising tx power.
   *     o Device name.
   *     o 16-bit service UUIDs (alert notifications).
   */

  memset(&fields, 0, sizeof fields);

  /* Advertise two flags:
   *     o Discoverability in forthcoming advertisement (general)
   *     o BLE-only (BR/EDR unsupported).
   */
  fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

  /* Indicate that the TX power level field should be included; have the
   * stack fill this value automatically.  This is done by assigning the
   * special value BLE_HS_ADV_TX_PWR_LVL_AUTO.
   */
  fields.tx_pwr_lvl_is_present = 1;
  fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;

  name = ble_svc_gap_device_name();
  fields.name = (uint8_t *)name;
  fields.name_len = strlen(name);
  fields.name_is_complete = 1;

  fields.uuids16 = (ble_uuid16_t[]){BLE_UUID16_INIT(BLE_SVC_SPP_UUID16)};
  fields.num_uuids16 = 1;
  fields.uuids16_is_complete = 1;

  rc = ble_gap_adv_set_fields(&fields);
  if (rc != 0) {
    MODLOG_DFLT(ERROR, "error setting advertisement data; rc=%d\n", rc);
    return;
  }

  /* Begin advertising. */
  memset(&adv_params, 0, sizeof adv_params);
  adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
  adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
  rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER, &adv_params,
                         ble_spp_server_gap_event, NULL);
  if (rc != 0) {
    MODLOG_DFLT(ERROR, "error enabling advertisement; rc=%d\n", rc);
    return;
  }
}

/**
 * The nimble host executes this callback when a GAP event occurs.  The
 * application associates a GAP event callback with each connection that forms.
 * ble_spp_server uses the same callback for all connections.
 *
 * @param event                 The type of event being signalled.
 * @param ctxt                  Various information pertaining to the event.
 * @param arg                   Application-specified argument; unused by
 *                                  ble_spp_server.
 *
 * @return                      0 if the application successfully handled the
 *                                  event; nonzero on failure.  The semantics
 *                                  of the return code is specific to the
 *                                  particular GAP event being signalled.
 */
static int ble_spp_server_gap_event(struct ble_gap_event *event, void *arg) {
  struct ble_gap_conn_desc desc;
  int rc;
  switch (event->type) {
  case BLE_GAP_EVENT_LINK_ESTAB:
    MODLOG_DFLT(INFO, "connection %s; status=%d ",
                event->link_estab.status == 0 ? "established" : "failed",
                event->link_estab.status);
    if (event->link_estab.status == 0) {
      rc = ble_gap_conn_find(event->link_estab.conn_handle, &desc);
      assert(rc == 0);
      ble_spp_server_print_conn_desc(&desc);
    }
    MODLOG_DFLT(INFO, "\n");
    if (event->link_estab.status != 0 || CONFIG_BT_NIMBLE_MAX_CONNECTIONS > 1) {
      if (get_number_of_connections() == CONFIG_BT_NIMBLE_MAX_CONNECTIONS) {
        int err_code_gap_adv = ble_gap_adv_stop();
        if (err_code_gap_adv != 0) {
          ESP_LOGI("BLE", "Unable to stop advertising");
        }
      } else {
        ble_spp_server_advertise();
      }
    }
    return 0;

  case BLE_GAP_EVENT_DISCONNECT:
    MODLOG_DFLT(INFO, "disconnect; reason=%d ", event->disconnect.reason);
    ble_spp_server_print_conn_desc(&event->disconnect.conn);
    MODLOG_DFLT(INFO, "\n");

    active_conn = BLE_HS_CONN_HANDLE_NONE;
    uart0_notify = false;
    uart1_notify = false;

    ble_spp_server_advertise();
    return 0;

  case BLE_GAP_EVENT_CONN_UPDATE:
    MODLOG_DFLT(INFO, "connection updated; status=%d ",
                event->conn_update.status);
    rc = ble_gap_conn_find(event->conn_update.conn_handle, &desc);
    assert(rc == 0);
    ble_spp_server_print_conn_desc(&desc);
    MODLOG_DFLT(INFO, "\n");
    return 0;

  case BLE_GAP_EVENT_ADV_COMPLETE:
    MODLOG_DFLT(INFO, "advertise complete; reason=%d",
                event->adv_complete.reason);
    ble_spp_server_advertise();
    return 0;

  case BLE_GAP_EVENT_MTU:
    MODLOG_DFLT(INFO, "mtu update event; conn_handle=%d cid=%d mtu=%d\n",
                event->mtu.conn_handle, event->mtu.channel_id,
                event->mtu.value);
    return 0;

  case BLE_GAP_EVENT_SUBSCRIBE:
    MODLOG_DFLT(INFO,
                "subscribe event; conn_handle=%d attr_handle=%d "
                "reason=%d prevn=%d curn=%d previ=%d curi=%d\n",
                event->subscribe.conn_handle, event->subscribe.attr_handle,
                event->subscribe.reason, event->subscribe.prev_notify,
                event->subscribe.cur_notify, event->subscribe.prev_indicate,
                event->subscribe.cur_indicate);
    active_conn = event->subscribe.conn_handle;

    if (event->subscribe.attr_handle == ble_spp_tx_handle_uart_0) {
      uart0_notify = event->subscribe.cur_notify;
    } else if (event->subscribe.attr_handle == ble_spp_tx_handle_uart_1) {
      uart1_notify = event->subscribe.cur_notify;
    }
    return 0;

  default:
    return 0;
  }
}

static void ble_spp_server_on_reset(int reason) {
  MODLOG_DFLT(ERROR, "Resetting state; reason=%d\n", reason);
}

static void ble_spp_server_on_sync(void) {
  int rc;

  rc = ble_hs_util_ensure_addr(0);
  assert(rc == 0);

  rc = ble_hs_id_infer_auto(0, &own_addr_type);
  if (rc != 0) {
    MODLOG_DFLT(ERROR, "error determining address type; rc=%d\n", rc);
    return;
  }

  uint8_t addr_val[6] = {0};
  rc = ble_hs_id_copy_addr(own_addr_type, addr_val, NULL);

  MODLOG_DFLT(INFO, "Device Address: ");
  print_addr(addr_val);
  MODLOG_DFLT(INFO, "\n");
  ble_spp_server_advertise();
}

void ble_spp_server_host_task(void *param) {
  MODLOG_DFLT(INFO, "BLE Host Task Started");
  nimble_port_run();

  nimble_port_freertos_deinit();
}

static int ble_svc_gatt_handler(uint16_t conn_handle, uint16_t attr_handle,
                                struct ble_gatt_access_ctxt *ctxt, void *arg) {
  switch (ctxt->op) {
  case BLE_GATT_ACCESS_OP_READ_CHR:
    MODLOG_DFLT(INFO, "Callback for read");
    break;
  case BLE_GATT_ACCESS_OP_WRITE_CHR:
    if (ctxt->om != NULL) {
      uint16_t total_len = OS_MBUF_PKTLEN(ctxt->om);

      uint8_t *temp_buf = (uint8_t *)malloc(total_len);
      if (temp_buf == NULL) {
        MODLOG_DFLT(ERROR, "Malloc failed for %d bytes\n", total_len);
        return BLE_ATT_ERR_INSUFFICIENT_RES;
      }

      os_mbuf_copydata(ctxt->om, 0, total_len, temp_buf);

      if (attr_handle == ble_spp_rx_handle_uart_0) {
        uart_write_bytes(UART_NUM_0, temp_buf, total_len);
        MODLOG_DFLT(INFO, "Sent %d bytes to UART0\n", total_len);
      } 
      else if (attr_handle == ble_spp_rx_handle_uart_1) {
        uart_write_bytes(UART_NUM_1, temp_buf, total_len);
        MODLOG_DFLT(INFO, "Sent %d bytes to UART1\n", total_len);
      }
      else if (attr_handle == ble_spp_handle_uart_0_baud_rate) 
      {
          uint32_t new_uart0_baud_rate; 
          memcpy(&new_uart0_baud_rate, temp_buf, sizeof(new_uart0_baud_rate));
          uart_set_baudrate(UART_NUM_0, new_uart0_baud_rate);  
      }
      else if (attr_handle == ble_spp_handle_uart_1_baud_rate) 
      {
        
          uint32_t new_uart1_baud_rate; 
          memcpy(&new_uart1_baud_rate, temp_buf, sizeof(new_uart1_baud_rate));
          uart_set_baudrate(UART_NUM_1, new_uart1_baud_rate);  
      }


      free(temp_buf);
    }
    break;

  default:
    MODLOG_DFLT(INFO, "\nDefault Callback");
    break;
  }
  return 0;
}

/* Define new custom service */
static const struct ble_gatt_svc_def new_ble_svc_gatt_defs[] = {
    {
        /*** Service: UART_1 SPP */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(BLE_SVC_SPP_UUID16),
        .characteristics =
            (struct ble_gatt_chr_def[]){
                {/* Support SPP service */
                 .uuid =
                     BLE_UUID16_DECLARE(BLE_SVC_SPP_CHR_UUID16_WRITE_UART_1),
                 .access_cb = ble_svc_gatt_handler,
                 .val_handle = &ble_spp_rx_handle_uart_1,
                 .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP},
                {

                    .uuid =
                        BLE_UUID16_DECLARE(BLE_SVC_SPP_CHR_UUID16_READ_UART_1),
                    .access_cb = ble_svc_gatt_handler,
                    .val_handle = &ble_spp_tx_handle_uart_1,
                    .flags = BLE_GATT_CHR_F_NOTIFY,
                },
                { 
                    .uuid =
                        BLE_UUID16_DECLARE(BLE_SVC_SPP_CHR_UUID16_UART_1_BAUD_RATE),
                    .access_cb = ble_svc_gatt_handler,
                    .val_handle = &ble_spp_handle_uart_1_baud_rate,
                    .flags = BLE_GATT_CHR_F_WRITE_NO_RSP,
                }},
    },
    {
        /*** Service: UART_0 SPP */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(BLE_SVC_SPP_UUID16_SECOND),
        .characteristics =
            (struct ble_gatt_chr_def[]){
                {/* Support SPP service */
                 .uuid =
                     BLE_UUID16_DECLARE(BLE_SVC_SPP_CHR_UUID16_WRITE_UART_0),
                 .access_cb = ble_svc_gatt_handler,
                 .val_handle = &ble_spp_rx_handle_uart_0,
                 .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP},
                {

                    .uuid =
                        BLE_UUID16_DECLARE(BLE_SVC_SPP_CHR_UUID16_READ_UART_0),
                    .access_cb = ble_svc_gatt_handler,
                    .val_handle = &ble_spp_tx_handle_uart_0,
                    .flags = BLE_GATT_CHR_F_NOTIFY,
                },
                { 
                    .uuid =
                        BLE_UUID16_DECLARE(BLE_SVC_SPP_CHR_UUID16_UART_0_BAUD_RATE),
                    .access_cb = ble_svc_gatt_handler,
                    .val_handle = &ble_spp_handle_uart_0_baud_rate,
                    .flags = BLE_GATT_CHR_F_WRITE_NO_RSP,
                }},
    },

    {0}, /* No more services. */
};

static void gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt,
                                 void *arg) {
  char buf[BLE_UUID_STR_LEN];

  switch (ctxt->op) {
  case BLE_GATT_REGISTER_OP_SVC:
    MODLOG_DFLT(DEBUG, "registered service %s with handle=%d\n",
                ble_uuid_to_str(ctxt->svc.svc_def->uuid, buf),
                ctxt->svc.handle);
    break;

  case BLE_GATT_REGISTER_OP_CHR:
    MODLOG_DFLT(DEBUG,
                "registering characteristic %s with "
                "def_handle=%d val_handle=%d\n",
                ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf),
                ctxt->chr.def_handle, ctxt->chr.val_handle);
    break;

  case BLE_GATT_REGISTER_OP_DSC:
    MODLOG_DFLT(DEBUG, "registering descriptor %s with handle=%d\n",
                ble_uuid_to_str(ctxt->dsc.dsc_def->uuid, buf),
                ctxt->dsc.handle);
    break;

  default:
    assert(0);
    break;
  }
}

int gatt_svr_init(void) {
  int rc = 0;
  ble_svc_gap_init();
  ble_svc_gatt_init();

  rc = ble_gatts_count_cfg(new_ble_svc_gatt_defs);

  if (rc != 0) {
    return rc;
  }

  rc = ble_gatts_add_svcs(new_ble_svc_gatt_defs);
  if (rc != 0) {
    return rc;
  }

  return 0;
}

void ble_server_uart_task(void *pvParameters) {

  char TAG[] = "RX -> BLE Task";

  UartConnection *connection = (UartConnection *)pvParameters;

  uart_event_t event;

  uint8_t data[512];

  while (1) {

    if (!xQueueReceive(connection->Uart_queue_handle, &event, portMAX_DELAY))
      continue;

    if (event.type != UART_DATA)
      continue;

    int len = uart_read_bytes(connection->Uart_port, data,
                              MIN(event.size, 512), pdMS_TO_TICKS(20));

    if (len <= 0)
      continue;

    ESP_LOG_BUFFER_HEXDUMP(TAG, data, len, ESP_LOG_INFO);
    ESP_LOGI(TAG, "UART%d -> BLE %d bytes", connection->Uart_port, len);

    bool subscribed = false;
    uint16_t tx_handle = 0;

    if (connection->Uart_port == UART_NUM_0) {
      subscribed = uart0_notify;

      tx_handle = ble_spp_tx_handle_uart_0;
    } else {
      subscribed = uart1_notify;

      tx_handle = ble_spp_tx_handle_uart_1;
    }

    if (!subscribed && active_conn == BLE_HS_CONN_HANDLE_NONE)
      continue;


    uint16_t mtu = ble_att_mtu(active_conn);

    int payload =
        (mtu > BLE_HEADER_SIZE) ? mtu - BLE_HEADER_SIZE : BLE_DEFAULT_MTU;

    for (int off = 0; off < len; off += payload) {
      int chunk = MIN(payload, len - off);

      struct os_mbuf *om = ble_hs_mbuf_from_flat(data + off, chunk);
      int rc = ble_gatts_notify_custom(active_conn, tx_handle, om);

      if (rc) {
        ESP_LOGW(TAG, "notify fail %d", rc);

        os_mbuf_free_chain(om);
      }
    }
  }
  taskYIELD();
}

void ble_spp_uart_init(UartConnection *uart_connection_attributes) {
  uart_config_t uart_config = {
      .baud_rate = 115200,
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
      .rx_flow_ctrl_thresh = 122,
      .source_clk = UART_SCLK_DEFAULT,
  };
  uart_driver_install(uart_connection_attributes->Uart_port, 16384, 16384, 10,
                      &uart_connection_attributes->Uart_queue_handle, 0);
  MODLOG_DFLT(INFO, "Queue addr: %p\n",
              uart_connection_attributes->Uart_queue_handle);
  uart_param_config(uart_connection_attributes->Uart_port, &uart_config);
  uart_set_pin(uart_connection_attributes->Uart_port,
               uart_connection_attributes->tx, uart_connection_attributes->rx,
               UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
  gpio_pullup_en(uart_connection_attributes->rx);
  xTaskCreate(ble_server_uart_task, "uTask", 4096,
              (void *)uart_connection_attributes, 5, NULL);
}

void app_main(void) {
  int rc;
  xTaskCreate(led_task, "uTaskBlink", 2048, (void *)&led_state, 2, NULL);
  Port0.Uart_port = UART_NUM_0;
  Port0.tx = GPIO_NUM_9;
  Port0.rx = GPIO_NUM_10;

  Port1.Uart_port = UART_NUM_1;
  Port1.tx = GPIO_NUM_4;
  Port1.rx = GPIO_NUM_5;

  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  ret = nimble_port_init();
  if (ret != ESP_OK) {
    MODLOG_DFLT(ERROR, "Failed to init nimble %d \n", ret);
    return;
  }

  uart0_notify = false;
  uart1_notify = false;

  ble_spp_uart_init(&Port0);
  ble_spp_uart_init(&Port1);
  ble_hs_cfg.reset_cb = ble_spp_server_on_reset;
  ble_hs_cfg.sync_cb = ble_spp_server_on_sync;
  ble_hs_cfg.gatts_register_cb = gatt_svr_register_cb;
  ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

  ble_hs_cfg.sm_io_cap = CONFIG_EXAMPLE_IO_TYPE;
#ifdef CONFIG_EXAMPLE_BONDING
  ble_hs_cfg.sm_bonding = 1;
#endif
#ifdef CONFIG_EXAMPLE_MITM
  ble_hs_cfg.sm_mitm = 1;
#endif
#ifdef CONFIG_EXAMPLE_USE_SC
  ble_hs_cfg.sm_sc = 1;
#endif
#ifdef CONFIG_EXAMPLE_BONDING
  ble_hs_cfg.sm_our_key_dist = 1;
  ble_hs_cfg.sm_their_key_dist = 1;
#endif
  /* Register custom service */
  rc = gatt_svr_init();
  assert(rc == 0);
  /* Set the default device name. */
  rc = ble_svc_gap_device_name_set(BLE_ADVERTISING_NAME);
  assert(rc == 0);
  /* XXX Need to have template for store */
  ble_store_config_init();
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, BLE_DEFAULT_POWER);
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, BLE_ADVERTISING_POWER);
  nimble_port_freertos_init(ble_spp_server_host_task);
  ble_att_set_preferred_mtu(BLE_PREFERRED_MTU);
}
