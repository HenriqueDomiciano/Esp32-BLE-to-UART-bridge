#include "host/ble_gap.h"
#include "sdkconfig.h"
#include <stdint.h>


uint8_t get_number_of_connections(void) {
  uint8_t counter = 0;
  for (int conn = 0; conn < CONFIG_BT_NIMBLE_MAX_CONNECTIONS + 1; conn++) {
    struct ble_gap_conn_desc desc;
    if (ble_gap_conn_find(conn, &desc) == 0) {
      counter++;
    }
  }
  return counter;
}