#include "ble.h"
#include "esp_peripheral.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#include <sys/types.h>
#include "driver/uart.h"
#include "services/gatt/ble_svc_gatt.h"


int ble_spp_server_gap_event(struct ble_gap_event *event, void *arg);
int gatt_svr_register(void);
static uint8_t own_addr_type;

bool uart0_notify = false;
bool uart1_notify = false;
uint16_t active_conn = BLE_HS_CONN_HANDLE_NONE;

#if CONFIG_IDF_TARGET_ESP32S3
bool uart2_notify = 0;
u_int16_t ble_spp_tx_handle_uart_2;
u_int16_t ble_spp_rx_handle_uart_2;
u_int16_t ble_spp_handle_uart_2_baud_rate;
#endif

u_int16_t ble_spp_tx_handle_uart_1;
u_int16_t ble_spp_rx_handle_uart_1;
u_int16_t ble_spp_tx_handle_uart_0;
u_int16_t ble_spp_rx_handle_uart_0;

u_int16_t ble_spp_handle_uart_1_baud_rate;
u_int16_t ble_spp_handle_uart_0_baud_rate;

uint8_t get_number_of_connections(void) {
  uint8_t counter = 0;
  for (int conn = 0; conn < CONFIG_BT_NIMBLE_MAX_CONNECTIONS + 1; conn++) {
    struct ble_gap_conn_desc desc;
    if (ble_gap_conn_find(conn, &desc) == 0) {
      counter++;
    }
  }
  return counter;
}

esp_power_level_t get_esp32_power_value_based_on_int(int8_t power_configuration) {

  esp_power_level_t power;

  switch (power_configuration) {
  case 9:
    power = ESP_PWR_LVL_P12;
    break;
  case 7:
    power = ESP_PWR_LVL_P9;
    break;
  case 5:
    power = ESP_PWR_LVL_P6;
    break;
  case 3:
    power = ESP_PWR_LVL_P3;
    break;
  case 0:
    power = ESP_PWR_LVL_N0;
    break;
  case -2:
    power = ESP_PWR_LVL_N3;
    break;
  case -6:
    power = ESP_PWR_LVL_N6;
    break;
  case -12:
    power = ESP_PWR_LVL_N12;
    break;
  default:
    power = ESP_PWR_LVL_P3;
    break;
  }
  return power;
}


/**
 * Logs information about a connection to the console.
 */
void ble_spp_server_print_conn_desc(struct ble_gap_conn_desc *desc) {
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

void ble_spp_server_advertise(void) {
  struct ble_gap_adv_params adv_params;
  struct ble_hs_adv_fields fields;
  const char *name;
  int rc;

  memset(&fields, 0, sizeof fields);

  fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

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

int ble_spp_server_gap_event(struct ble_gap_event *event, void *arg) 
{
  struct ble_gap_conn_desc desc;
  int rc;
  switch (event->type) {

  case BLE_GAP_EVENT_CONNECT:
    active_conn = event->connect.conn_handle;
#ifdef CONFIG_ENABLE_2M_PHY
    if (event->connect.status == 0) {
      ble_gap_set_prefered_le_phy(event->connect.conn_handle,
                                  BLE_GAP_LE_PHY_2M_MASK,
                                  BLE_GAP_LE_PHY_2M_MASK, 0);
    }
#endif
    struct ble_gap_upd_params params = {.itvl_min = 6, // 7.5 ms
                                        .itvl_max = 6, // 15 ms
                                        .latency = 0,
                                        .supervision_timeout = 200};
    ble_gap_update_params(active_conn, &params);
    return 0;
  case BLE_GAP_EVENT_PHY_UPDATE_COMPLETE:
    ESP_LOGI("BLE", "TX PHY: %d RX PHY: %d", event->phy_updated.tx_phy,
             event->phy_updated.rx_phy);
    return 0;
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
#if CONFIG_IDF_TARGET_ESP32S3
    else if (event->subscribe.attr_handle == ble_spp_tx_handle_uart_2) {
      uart2_notify = event->subscribe.cur_notify;
    }
#endif
    return 0;

  default:
    return 0;
  }
}

void ble_spp_server_on_reset(int reason) {
  MODLOG_DFLT(ERROR, "Resetting state; reason=%d\n", reason);
}

void ble_spp_server_on_sync(void) {
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


int ble_svc_gatt_handler(uint16_t conn_handle, uint16_t attr_handle,
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
      } else if (attr_handle == ble_spp_rx_handle_uart_1) {
        uart_write_bytes(UART_NUM_1, temp_buf, total_len);
        MODLOG_DFLT(INFO, "Sent %d bytes to UART1\n", total_len);
      }
#if CONFIG_IDF_TARGET_ESP32S3
      else if (attr_handle == ble_spp_rx_handle_uart_2) {
        uart_write_bytes(UART_NUM_2, temp_buf, total_len);
        MODLOG_DFLT(INFO, "Sent %d bytes to UART2\n", total_len);
      }
#endif
      else if (attr_handle == ble_spp_handle_uart_0_baud_rate) {
        uint32_t new_uart0_baud_rate;
        memcpy(&new_uart0_baud_rate, temp_buf, sizeof(new_uart0_baud_rate));
        uart_set_baudrate(UART_NUM_0, new_uart0_baud_rate);
      } else if (attr_handle == ble_spp_handle_uart_1_baud_rate) {

        uint32_t new_uart1_baud_rate;
        memcpy(&new_uart1_baud_rate, temp_buf, sizeof(new_uart1_baud_rate));
        uart_set_baudrate(UART_NUM_1, new_uart1_baud_rate);
      }
#if CONFIG_IDF_TARGET_ESP32S3
      else if (attr_handle == ble_spp_handle_uart_2_baud_rate) {
        uint32_t new_uart2_baud_rate;
        memcpy(&new_uart2_baud_rate, temp_buf, sizeof(new_uart2_baud_rate));
        uart_set_baudrate(UART_NUM_2, new_uart2_baud_rate);
      }
#endif

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
                    .uuid = BLE_UUID16_DECLARE(
                        BLE_SVC_SPP_CHR_UUID16_UART_1_BAUD_RATE),
                    .access_cb = ble_svc_gatt_handler,
                    .val_handle = &ble_spp_handle_uart_1_baud_rate,
                    .flags = BLE_GATT_CHR_F_WRITE_NO_RSP,
                },
                {0}},
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
                    .uuid = BLE_UUID16_DECLARE(
                        BLE_SVC_SPP_CHR_UUID16_UART_0_BAUD_RATE),
                    .access_cb = ble_svc_gatt_handler,
                    .val_handle = &ble_spp_handle_uart_0_baud_rate,
                    .flags = BLE_GATT_CHR_F_WRITE_NO_RSP,
                },
                {0}},
    },
#if CONFIG_IDF_TARGET_ESP32S3
    {
        /*** Service: UART_2 SPP */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(BLE_SVC_SPP_UUID16_THIRD),
        .characteristics =
            (struct ble_gatt_chr_def[]){
                {/* Support SPP service */
                 .uuid =
                     BLE_UUID16_DECLARE(BLE_SVC_SPP_CHR_UUID16_WRITE_UART_2),
                 .access_cb = ble_svc_gatt_handler,
                 .val_handle = &ble_spp_rx_handle_uart_2,
                 .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP},
                {

                    .uuid =
                        BLE_UUID16_DECLARE(BLE_SVC_SPP_CHR_UUID16_READ_UART_2),
                    .access_cb = ble_svc_gatt_handler,
                    .val_handle = &ble_spp_tx_handle_uart_2,
                    .flags = BLE_GATT_CHR_F_NOTIFY,
                },
                {
                    .uuid = BLE_UUID16_DECLARE(
                        BLE_SVC_SPP_CHR_UUID16_UART_2_BAUD_RATE),
                    .access_cb = ble_svc_gatt_handler,
                    .val_handle = &ble_spp_handle_uart_2_baud_rate,
                    .flags = BLE_GATT_CHR_F_WRITE_NO_RSP,
                },
                {0}},
    },
#endif
    {0}, /* No more services. */
};

void gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt,
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

