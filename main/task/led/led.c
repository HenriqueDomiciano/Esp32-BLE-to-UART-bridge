#include "led.h"
#include "../ble/ble.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "soc/gpio_num.h"
#include <math.h>
#include <stdint.h>

#if CONFIG_IDF_TARGET_ESP32S3 && CONFIG_USE_RGB_LED
#include "led_strip.h"
static led_strip_handle_t led_strip;
static float_t breathing_speed = 0.2f;
static uint8_t max_brightness = 100; 

void rgb_init(void) {
  led_strip_config_t strip_config = {
      .strip_gpio_num = GPIO_NUM_48,
      .max_leds = 1,
  };

  led_strip_rmt_config_t rmt_config = {
      .resolution_hz = 10 * 1000 * 1000, // 10 MHz
  };

  led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip);
  led_strip_clear(led_strip); 
  led_strip_set_pixel(led_strip, 0, 0, 0,0);
  vTaskDelay(pdMS_TO_TICKS(30)); 
}
void rgb_set(uint8_t r, uint8_t g, uint8_t b) {
  led_strip_set_pixel(led_strip, 0, r, g, b);
  led_strip_refresh(led_strip);
}
#endif

#if CONFIG_IDF_TARGET_ESP32S3
static blinkParameters_t default_blink_parameter = {GPIO_NUM_48, 50, 50};
#else
static blinkParameters_t default_blink_parameter = {GPIO_NUM_8, 50, 50};
#endif

blinkParameters_t get_current_ble_state_config(BleState_e *current_state) {
  BleState_e state = (BleState_e)*current_state;
  switch (state) {
  case CONNECTED:
    return (blinkParameters_t){default_blink_parameter.pin, 0, 5000};
  case DISCONNECTED:
    return default_blink_parameter;
  case WAITING:
    return (blinkParameters_t){default_blink_parameter.pin, 500, 500};
  default:
    return default_blink_parameter;
  }
}

void led_task(void *pvParameters) {
  BleState_e *led_state = (BleState_e *)pvParameters;
  blinkParameters_t state = get_current_ble_state_config(led_state);
  uint8_t number_of_connections;
#if !CONFIG_IDF_TARGET_ESP32S3
  gpio_reset_pin(state.pin);
#endif
#if CONFIG_IDF_TARGET_ESP32S3 && CONFIG_USE_RGB_LED
  rgb_init();
  static float t = 0.0f;
#else
  gpio_set_direction(state.pin, GPIO_MODE_OUTPUT_OD);
#endif
  int64_t starting_time = esp_timer_get_time();
  int64_t time_difference;
  int64_t current_time;
  while (1) {
    current_time = esp_timer_get_time();
    time_difference = current_time - starting_time;
    if (time_difference >= LED_CONNECTION_UPDATE_TIME) {
      starting_time = current_time;
      number_of_connections = get_number_of_connections();
      ESP_LOGI("LED", "Number of connections %d", number_of_connections);
      if (number_of_connections == 0) {
        *led_state = WAITING;
      } else {
        *led_state = CONNECTED;
      }
    } else {
#if CONFIG_IDF_TARGET_ESP32S3 && CONFIG_USE_RGB_LED
    t += breathing_speed;
    if (t > 2 * M_PI) t -= 2 * M_PI;      
    float wave = (sinf(t) + 1.0f) * 0.5f;
    float final_wave = wave * wave; 
    
    // scale brightness (avoid too low values!)
    uint8_t b = (uint8_t)(final_wave * max_brightness);  // 0–100 safe

    uint8_t r = 0, g = 0, bl = 0;
    
    if (*led_state == WAITING) 
    {
        r = b; 
        bl = b/2;
    } 
    else if (*led_state == CONNECTED) 
    {
        g = b; 
    }
    else {
        r = b; 
    }
      rgb_set(r,g,bl); 
      vTaskDelay(pdMS_TO_TICKS(30));
#else
      state = get_current_ble_state_config(led_state);
      gpio_set_level(state.pin, 1);
      vTaskDelay(pdMS_TO_TICKS(state.time_on_ms));

      gpio_set_level(state.pin, 0);
      vTaskDelay(pdMS_TO_TICKS(state.time_off_ms));
#endif
    }
  }
}