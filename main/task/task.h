#pragma once

#include "freertos/idf_additions.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TASK_HIGH_PRIORITY 15
#define TASK_MEDIUM_PRIORITY 5
#define TASK_LOW_PRIORITY 2

struct UartTxData
{
    uint8_t *data;
    size_t len;
} typedef UartTxData;


struct BleHandle_t {
  uint16_t tx_handle;
  uint16_t rx_handle;
  uint16_t baud_rate_handle;
  bool uart_notify;
  QueueHandle_t uart_tx_queue;
} typedef BleHandle;