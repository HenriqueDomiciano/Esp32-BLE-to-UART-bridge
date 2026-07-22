#ifndef H_BLESPPSERVER_
#define H_BLESPPSERVER_
#endif


#include "driver/gpio.h"
#include <assert.h>
#include "esp_log.h"
#include "esp_peripheral.h"
#include "esp_timer.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "hal/gpio_types.h"
#include "hal/uart_types.h"
#include "host/ble_gap.h"
#include "host/ble_hs_adv.h"
#include "nvs_flash.h"
#include "task/ble/ble.h"
#include "task/led/led.h"
#include "task/uart/uart.h"
/* BLE */
#include "driver/uart.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "soc/gpio_num.h"
#include "esp_bt.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include "esp_assert.h"  
#include "sys/param.h"  
#include <stdbool.h>
#include <stdint.h>
#include "freertos/idf_additions.h"
#include "hal/uart_types.h"
#include "nimble/ble.h"
#include "soc/gpio_num.h"
#include "sdkconfig.h"
#include "esp_bt.h"

#ifdef __cplusplus
extern "C" {
#endif


