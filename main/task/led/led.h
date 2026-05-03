#include "soc/gpio_num.h"
#include <stdint.h> 

#define LED_CONNECTION_UPDATE_TIME                                       1000000


struct blink_structure
{
    gpio_num_t pin;
    uint32_t time_on_ms; 
    uint32_t time_off_ms; 
} typedef blinkParameters_t ;


enum {
    CONNECTED,
    DISCONNECTED,
    WAITING
} typedef BleState_e ;


void led_task(void *pvParameters); 
blinkParameters_t get_current_ble_state_config(BleState_e *current_state);