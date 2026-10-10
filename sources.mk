SENSOR_DIR ?= $(dir $(lastword $(MAKEFILE_LIST)))
SENSOR_SOURCES := $(addprefix $(SENSOR_DIR)/, app_main.c app_ui.c sensor_control.c sensor_engine.c battery.c battery_adc.c battery_model.c board_io.c zcl_clusters.c zcl_callbacks.c zcl_guard.c zigbee_callbacks.c)
