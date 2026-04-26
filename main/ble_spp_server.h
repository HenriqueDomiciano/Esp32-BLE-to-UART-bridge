#ifndef H_BLESPPSERVER_
#define H_BLESPPSERVER_
#endif

#include <stdbool.h>
#include <stdint.h>
#include "freertos/idf_additions.h"
#include "hal/uart_types.h"
#include "nimble/ble.h"
#include "soc/gpio_num.h"
#ifdef __cplusplus
extern "C" {
#endif

/* 16 Bit SPP Service UUID */
#define BLE_SVC_SPP_UUID16                                  0xABF0
#define BLE_SVC_SPP_UUID16_SECOND                           0xABE0
/* 16 Bit SPP Service Characteristic UUID */

#define BLE_SVC_SPP_CHR_UUID16_WRITE_UART_1                              0xABF1
#define BLE_SVC_SPP_CHR_UUID16_READ_UART_1                               0xABF2 

#define BLE_SVC_SPP_CHR_UUID16_WRITE_UART_0                              0xABE1
#define BLE_SVC_SPP_CHR_UUID16_READ_UART_0                               0xABE2 

#define BLE_DEFAULT_MTU                                                  20
#define BLE_PREFERRED_MTU                                                517
#define BLE_HEADER_SIZE                                                  3

#define BLE_ADVERTISING_NAME                                                 "UART-to-BLE-Bridge"

struct ble_hs_cfg;
struct ble_gatt_register_ctxt;

struct uart_connection_attributes
{
    QueueHandle_t Uart_queue_handle; 
    uart_port_t Uart_port;
    gpio_num_t rx;
    gpio_num_t tx;


} typedef UartConnection ;

struct{
    bool uart0_notify;
    bool uart1_notify;
} typedef connectionsubscription_t;

