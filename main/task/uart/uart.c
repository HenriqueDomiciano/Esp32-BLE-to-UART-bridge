
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "host/ble_att.h"
#include "host/ble_hs.h"
#include "uart.h"
#include <sys/param.h>
#include <sys/types.h>


extern bool uart0_notify;
extern bool uart1_notify;
extern uint16_t active_conn; 

#if CONFIG_IDF_TARGET_ESP32S3
extern bool uart2_notify;
extern u_int16_t ble_spp_tx_handle_uart_2;
extern u_int16_t ble_spp_rx_handle_uart_2;
extern u_int16_t ble_spp_handle_uart_2_baud_rate;
#endif

extern u_int16_t ble_spp_tx_handle_uart_1;
extern u_int16_t ble_spp_rx_handle_uart_1;
extern u_int16_t ble_spp_tx_handle_uart_0;
extern u_int16_t ble_spp_rx_handle_uart_0;

void ble_server_uart_task(void *pvParameters) {
  UartConnection *connection = (UartConnection *)pvParameters;

  uart_event_t event;

  uint8_t aggregate[2048];
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
        subscribed = uart0_notify;
        tx_handle = ble_spp_tx_handle_uart_0;
      } else if (connection->Uart_port == UART_NUM_1) {
        subscribed = uart1_notify;
        tx_handle = ble_spp_tx_handle_uart_1;
      }

#if CONFIG_IDF_TARGET_ESP32S3
      else if (connection->Uart_port == UART_NUM_2) {
        subscribed = uart2_notify;
        tx_handle = ble_spp_tx_handle_uart_2;
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
  xTaskCreate(ble_server_uart_task, "uTask", 4096,
              (void *)uart_connection_attributes, 10, NULL);
}