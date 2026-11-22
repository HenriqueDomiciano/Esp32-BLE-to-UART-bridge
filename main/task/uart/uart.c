
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "host/ble_att.h"
#include "host/ble_hs.h"
#include "uart.h"
#include <sys/param.h>
#include <sys/types.h>


extern uint16_t active_conn; 

#if CONFIG_IDF_TARGET_ESP32S3
#endif

extern BleHandle ble_spp_uart_0_handles;
extern BleHandle ble_spp_uart_1_handles;
#if CONFIG_IDF_TARGET_ESP32S3
extern BleHandle ble_spp_uart_2_handles;
#endif


/**
 * @brief Task send  data to BLE the received UART from the device .
 *
 * Receives data from the UART
 *
 * @param[in] void *pvParameters pointer to UART connection attributes.
 *
 */
void ble_server_uart_rx_task(void *pvParameters) {
  UartConnection *connection = (UartConnection *)pvParameters;

  uart_event_t event;

  uint8_t aggregate[4096] = {0};
  size_t aggregate_len = 0;
  int64_t last_rx = 0;

  while (1) {
    if (xQueueReceive(connection->Uart_queue_handle, &event,
                      pdMS_TO_TICKS(20))) {
      if (event.type == UART_DATA) {
        size_t available = 0;

        uart_get_buffered_data_len(connection->Uart_port, &available);

        while (available > 0) {
          int len = uart_read_bytes(
              connection->Uart_port, aggregate + aggregate_len,
              MIN(available, sizeof(aggregate) - aggregate_len), 0);

          if (len <= 0)
            break;

          aggregate_len += len;

          uart_get_buffered_data_len(connection->Uart_port, &available);
        }

        last_rx = esp_timer_get_time();
      }
    }

    bool timeout = aggregate_len > 0 &&
                   (esp_timer_get_time() - last_rx > UART_IDLE_TIMEOUT_US);

    if (timeout) {
      bool subscribed = false;
      uint16_t tx_handle = 0;
      if (connection->Uart_port == UART_NUM_0) {
        subscribed = ble_spp_uart_0_handles.uart_notify;
        tx_handle = ble_spp_uart_0_handles.tx_handle;
      } else if (connection->Uart_port == UART_NUM_1) {
        subscribed = ble_spp_uart_1_handles.uart_notify;
        tx_handle = ble_spp_uart_1_handles.tx_handle;
      }

#if CONFIG_IDF_TARGET_ESP32S3
      else if (connection->Uart_port == UART_NUM_2) {
        subscribed = ble_spp_uart_2_handles.uart_notify;
        tx_handle = ble_spp_uart_2_handles.tx_handle;
      }
#endif

      if (subscribed && active_conn != BLE_HS_CONN_HANDLE_NONE) {
        uint16_t mtu = ble_att_mtu(active_conn);

        int payload = (mtu > 3) ? (mtu - 3) : 20;

        for (int off = 0; off < aggregate_len; off += payload) {
          int chunk = MIN(payload, aggregate_len - off);

          struct os_mbuf *om = ble_hs_mbuf_from_flat(aggregate + off, chunk);

          int rc = ble_gatts_notify_custom(active_conn, tx_handle, om);

          if (rc) {
            os_mbuf_free_chain(om);
          }
        }
      }

      aggregate_len = 0;
    }
  }
}

/**
 * @brief Task received BLE data to the UART transmit .
 *
 * Copies the payload fro write attribute in to UART
 * the NimBLE host task.
 *
 * @param[in] void *pvParameters pointer to UART connection attributes.
 *
 */
void ble_server_uart_tx_task(void *pvParameters) {
  
  #ifdef CONFIG_IDF_TARGET_ESP32S3
  QueueHandle_t uart_map[3] = {ble_spp_uart_0_handles.uart_tx_queue, ble_spp_uart_1_handles.uart_tx_queue, ble_spp_uart_2_handles.uart_tx_queue};
  #else
  QueueHandle_t uart_map[2] = {ble_spp_uart_0_handles.uart_tx_queue, ble_spp_uart_1_handles.uart_tx_queue};
  #endif
  UartConnection *connection = (UartConnection *)pvParameters;

  UartTxData tx_data;

  while (1) {
    if (xQueueReceive(uart_map[connection->Uart_port], &tx_data, portMAX_DELAY)) {
      uart_write_bytes(connection->Uart_port, (const char *)tx_data.data,
                       tx_data.len);
    }
  }
}


/**
 * @brief function to initialize tasks for handling BLE data to the UART and received UART data.
 *
 * Copies the payload fro write attribute in to UART
 * the NimBLE host task.
 *
 * @param[in] void *UartConnection pointer to UART connection attributes.
 *
 */
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
  uart_set_rx_timeout(uart_connection_attributes->Uart_port, 1);
  uart_driver_install(uart_connection_attributes->Uart_port, 16384, 16384, 20,
                      &uart_connection_attributes->Uart_queue_handle, 0);
  uart_set_always_rx_timeout(uart_connection_attributes->Uart_port, false);
  uart_param_config(uart_connection_attributes->Uart_port, &uart_config);
  uart_set_pin(uart_connection_attributes->Uart_port,
               uart_connection_attributes->tx, uart_connection_attributes->rx,
               UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

  gpio_pullup_en(uart_connection_attributes->rx);
  xTaskCreate(ble_server_uart_rx_task, "uTaskRX", 4096,
              (void *)uart_connection_attributes, TASK_HIGH_PRIORITY, NULL);
  xTaskCreate(ble_server_uart_tx_task, "uTaskTX", 4096,
              (void *)uart_connection_attributes, TASK_HIGH_PRIORITY, NULL);
}