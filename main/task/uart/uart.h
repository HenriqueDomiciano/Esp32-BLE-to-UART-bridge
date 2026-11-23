#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/idf_additions.h"
#include "task.h"


#define UART_IDLE_TIMEOUT_US 5000



struct UartConnection
{
    QueueHandle_t Uart_queue_handle;
    uart_port_t Uart_port;
    gpio_num_t rx;
    gpio_num_t tx;

} typedef UartConnection;

struct TxTaskParameters
{
    UartConnection *uart_connection;
    QueueHandle_t *uart_tx_queue;
} typedef TxTaskParameters;


void ble_spp_uart_init(UartConnection *uart_connection_attributes);
void ble_server_uart_rx_task(void *pvParameters);
void ble_server_uart_tx_task(void *pvParameters);