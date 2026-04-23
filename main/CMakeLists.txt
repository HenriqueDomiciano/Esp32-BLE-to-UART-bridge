set(srcs "main.c")

idf_component_register(SRCS "${srcs}" "task/led/led.c" "task/ble/ble.c"
                       INCLUDE_DIRS "." "task/led" "task/ble")
