#include <stdint.h>
#include "host/ble_gap.h"
#include "sdkconfig.h"
#include <stdint.h>
#include "esp_bt.h"

uint8_t get_number_of_connections(void);
esp_power_level_t get_esp32_power_value_based_on_int(int8_t power_configuration); 
int ble_spp_server_gap_event(struct ble_gap_event *event, void *arg);
void ble_spp_server_advertise(void);
void ble_spp_server_on_sync(void);
void gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt,void *arg); 
void ble_spp_server_on_reset(int reason) ;
int gatt_svr_init(void);

#if CONFIG_IDF_TARGET_ESP32S3     
    #define BLE_SVC_SPP_UUID16_THIRD                                         0xAC00
    #define BLE_SVC_SPP_CHR_UUID16_UART_2_BAUD_RATE                          0xAC03
    #define BLE_SVC_SPP_CHR_UUID16_WRITE_UART_2                              0xAC01
    #define BLE_SVC_SPP_CHR_UUID16_READ_UART_2                               0xAC02 
#endif


/* 16 Bit SPP Service UUID */
#define BLE_SVC_SPP_UUID16                                  0xABF0
#define BLE_SVC_SPP_UUID16_SECOND                           0xABE0
/* 16 Bit SPP Service Characteristic UUID */

#define BLE_SVC_SPP_CHR_UUID16_WRITE_UART_1                              0xABF1
#define BLE_SVC_SPP_CHR_UUID16_READ_UART_1                               0xABF2 

#define BLE_SVC_SPP_CHR_UUID16_WRITE_UART_0                              0xABE1
#define BLE_SVC_SPP_CHR_UUID16_READ_UART_0                               0xABE2 

#define BLE_SVC_SPP_CHR_UUID16_UART_0_BAUD_RATE                          0xABE3
#define BLE_SVC_SPP_CHR_UUID16_UART_1_BAUD_RATE                          0XABF3

#define BLE_DEFAULT_MTU                                                  20
#define BLE_PREFERRED_MTU                                                517
#define BLE_HEADER_SIZE                                                  3

#define BLE_ADVERTISING_NAME                                                 "UART-to-BLE-Bridge"


#define BLE_ADVERTISING_POWER                                           ESP_PWR_LVL_P3  //3dm
#define BLE_DEFAULT_POWER                                                ESP_PWR_LVL_P3 //3dbm

struct ble_hs_cfg;
struct ble_gatt_register_ctxt;

