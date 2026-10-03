# Application sources only. Do not compile the original sampleLight app alongside these.
# SMART_PLUG_DIR must point to this directory; append SMART_PLUG_SOURCES to your SDK source list.
SMART_PLUG_DIR ?= $(dir $(lastword $(MAKEFILE_LIST)))
SMART_PLUG_SOURCES := $(addprefix $(SMART_PLUG_DIR)/, \
    app_main.c \
    app_ui.c \
    plug_control.c \
    hlw8032.c \
    bl0937.c \
    smart_plug.c \
    zcl_clusters.c \
    zcl_callbacks.c \
    zcl_guard.c \
    zcl_on_off.c \
    zcl_scenes.c \
    zigbee_callbacks.c)
