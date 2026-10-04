# Application sources only. Do not compile the original sampleLight app alongside these.
# RELAY_2CH_DIR must point to this directory; append RELAY_2CH_SOURCES to your SDK source list.
RELAY_2CH_DIR ?= $(dir $(lastword $(MAKEFILE_LIST)))
RELAY_2CH_SOURCES := $(addprefix $(RELAY_2CH_DIR)/, \
    app_main.c \
    app_ui.c \
    relay_control.c \
    relay_2ch.c \
    button_engine.c \
    board_io.c \
    zcl_clusters.c \
    zcl_callbacks.c \
    zcl_guard.c \
    zcl_on_off.c \
    zcl_scenes.c \
    zigbee_callbacks.c)
