#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"


#define UART_IDLE_TIMEOUT_US 5000


struct uart_connection_attributes
{
    QueueHandle_t Uart_queue_handle; 
    uart_port_t Uart_port;
    gpio_num_t rx;
    gpio_num_t tx;


} typedef UartConnection;

void ble_spp_uart_init(UartConnection *uart_connection_attributes);
void ble_server_uart_task(void *pvParameters);