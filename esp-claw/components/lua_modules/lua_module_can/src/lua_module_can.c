/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#include "lua_module_can.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "cap_lua.h"
#include "driver/gpio.h"
#include "driver/twai.h"
#include "freertos/FreeRTOS.h"
#include "lauxlib.h"

#define CAN_METATABLE "can.handle"
#define CAN_MAX_DATA_LENGTH 8

typedef struct {
    twai_handle_t handle;
    bool open;
} can_ud_t;

static can_ud_t *check_open(lua_State *L, int index)
{
    can_ud_t *ud = (can_ud_t *)luaL_checkudata(L, index, CAN_METATABLE);
    if (!ud->open || ud->handle == NULL) {
        luaL_error(L, "can: handle is closed");
    }
    return ud;
}

static uint32_t check_timeout(lua_State *L, int index, uint32_t default_value)
{
    lua_Integer value = luaL_optinteger(L, index, (lua_Integer)default_value);
    if (value < 0 || (uint64_t)value > UINT32_MAX) {
        luaL_error(L, "can: timeout must be between 0 and %u ms", UINT32_MAX);
    }
    return (uint32_t)value;
}

static bool table_bool(lua_State *L, int index, const char *field, bool default_value)
{
    bool value = default_value;
    lua_getfield(L, index, field);
    if (!lua_isnil(L, -1)) {
        value = lua_toboolean(L, -1);
    }
    lua_pop(L, 1);
    return value;
}

static twai_timing_config_t get_timing(lua_State *L, lua_Integer bitrate)
{
    switch (bitrate) {
    case 25000:
        return (twai_timing_config_t)TWAI_TIMING_CONFIG_25KBITS();
    case 50000:
        return (twai_timing_config_t)TWAI_TIMING_CONFIG_50KBITS();
    case 100000:
        return (twai_timing_config_t)TWAI_TIMING_CONFIG_100KBITS();
    case 125000:
        return (twai_timing_config_t)TWAI_TIMING_CONFIG_125KBITS();
    case 250000:
        return (twai_timing_config_t)TWAI_TIMING_CONFIG_250KBITS();
    case 500000:
        return (twai_timing_config_t)TWAI_TIMING_CONFIG_500KBITS();
    case 800000:
        return (twai_timing_config_t)TWAI_TIMING_CONFIG_800KBITS();
    case 1000000:
        return (twai_timing_config_t)TWAI_TIMING_CONFIG_1MBITS();
    default:
        luaL_error(L,
                   "can.new: unsupported bitrate %d (use 25000, 50000, 100000, "
                   "125000, 250000, 500000, 800000, or 1000000)",
                   (int)bitrate);
        return (twai_timing_config_t)TWAI_TIMING_CONFIG_500KBITS();
    }
}

static size_t read_data(lua_State *L, int index, uint8_t *data)
{
    if (lua_type(L, index) == LUA_TSTRING) {
        size_t length = 0;
        const char *bytes = lua_tolstring(L, index, &length);
        if (length > CAN_MAX_DATA_LENGTH) {
            luaL_error(L, "can transmit: classical CAN payload cannot exceed 8 bytes");
        }
        memcpy(data, bytes, length);
        return length;
    }

    luaL_checktype(L, index, LUA_TTABLE);
    size_t length = lua_rawlen(L, index);
    if (length > CAN_MAX_DATA_LENGTH) {
        luaL_error(L, "can transmit: classical CAN payload cannot exceed 8 bytes");
    }
    for (size_t i = 0; i < length; ++i) {
        lua_rawgeti(L, index, (lua_Integer)i + 1);
        lua_Integer value = luaL_checkinteger(L, -1);
        if (value < 0 || value > UINT8_MAX) {
            luaL_error(L, "can transmit: data byte %u must be between 0 and 255",
                       (unsigned)i + 1);
        }
        data[i] = (uint8_t)value;
        lua_pop(L, 1);
    }
    return length;
}

static void close_driver(can_ud_t *ud)
{
    if (!ud->open || ud->handle == NULL) {
        return;
    }

    twai_status_info_t status = {0};
    if (twai_get_status_info_v2(ud->handle, &status) == ESP_OK &&
        status.state == TWAI_STATE_RUNNING) {
        (void)twai_stop_v2(ud->handle);
    }
    (void)twai_driver_uninstall_v2(ud->handle);
    ud->handle = NULL;
    ud->open = false;
}

static int can_close(lua_State *L)
{
    can_ud_t *ud = (can_ud_t *)luaL_checkudata(L, 1, CAN_METATABLE);
    if (!ud->open) {
        return 0;
    }

    twai_status_info_t status = {0};
    esp_err_t err = twai_get_status_info_v2(ud->handle, &status);
    if (err == ESP_OK && status.state == TWAI_STATE_RUNNING) {
        err = twai_stop_v2(ud->handle);
    }
    if (err == ESP_OK) {
        err = twai_driver_uninstall_v2(ud->handle);
    }
    if (err != ESP_OK) {
        return luaL_error(L, "can close failed: %s", esp_err_to_name(err));
    }

    ud->handle = NULL;
    ud->open = false;
    return 0;
}

static int can_gc(lua_State *L)
{
    can_ud_t *ud = (can_ud_t *)luaL_testudata(L, 1, CAN_METATABLE);
    if (ud) {
        close_driver(ud);
    }
    return 0;
}

static int can_transmit(lua_State *L)
{
    can_ud_t *ud = check_open(L, 1);
    lua_Integer identifier = luaL_checkinteger(L, 2);
    bool extended = false;
    bool rtr = false;
    bool single_shot = false;
    bool self_receive = false;
    uint32_t timeout_ms = 100;

    if (!lua_isnoneornil(L, 4)) {
        luaL_checktype(L, 4, LUA_TTABLE);
        extended = table_bool(L, 4, "extended", false);
        rtr = table_bool(L, 4, "rtr", false);
        single_shot = table_bool(L, 4, "single_shot", false);
        self_receive = table_bool(L, 4, "self_receive", false);
        lua_getfield(L, 4, "timeout_ms");
        if (!lua_isnil(L, -1)) {
            lua_Integer value = luaL_checkinteger(L, -1);
            if (value < 0 || (uint64_t)value > UINT32_MAX) {
                return luaL_error(L, "can transmit: invalid timeout_ms");
            }
            timeout_ms = (uint32_t)value;
        }
        lua_pop(L, 1);
    }

    lua_Integer max_identifier = extended ? 0x1FFFFFFF : 0x7FF;
    if (identifier < 0 || identifier > max_identifier) {
        return luaL_error(L, "can transmit: identifier out of range for %s frame",
                          extended ? "extended" : "standard");
    }

    twai_message_t message = {0};
    message.identifier = (uint32_t)identifier;
    message.data_length_code = (uint8_t)read_data(L, 3, message.data);
    if (extended) {
        message.flags |= TWAI_MSG_FLAG_EXTD;
    }
    if (rtr) {
        message.flags |= TWAI_MSG_FLAG_RTR;
    }
    if (single_shot) {
        message.flags |= TWAI_MSG_FLAG_SS;
    }
    if (self_receive) {
        message.flags |= TWAI_MSG_FLAG_SELF;
    }

    esp_err_t err = twai_transmit_v2(ud->handle, &message, pdMS_TO_TICKS(timeout_ms));
    if (err != ESP_OK) {
        return luaL_error(L, "can transmit failed: %s", esp_err_to_name(err));
    }
    lua_pushboolean(L, true);
    return 1;
}

static int can_receive(lua_State *L)
{
    can_ud_t *ud = check_open(L, 1);
    uint32_t timeout_ms = check_timeout(L, 2, 0);
    twai_message_t message = {0};
    esp_err_t err = twai_receive_v2(ud->handle, &message, pdMS_TO_TICKS(timeout_ms));
    if (err == ESP_ERR_TIMEOUT) {
        lua_pushnil(L);
        return 1;
    }
    if (err != ESP_OK) {
        return luaL_error(L, "can receive failed: %s", esp_err_to_name(err));
    }

    uint8_t length = message.data_length_code <= CAN_MAX_DATA_LENGTH
                         ? message.data_length_code : CAN_MAX_DATA_LENGTH;
    lua_createtable(L, 0, 6);
    lua_pushinteger(L, message.identifier);
    lua_setfield(L, -2, "id");
    lua_pushboolean(L, (message.flags & TWAI_MSG_FLAG_EXTD) != 0);
    lua_setfield(L, -2, "extended");
    lua_pushboolean(L, (message.flags & TWAI_MSG_FLAG_RTR) != 0);
    lua_setfield(L, -2, "rtr");
    lua_pushinteger(L, message.data_length_code);
    lua_setfield(L, -2, "dlc");

    lua_createtable(L, length, 0);
    for (uint8_t i = 0; i < length; ++i) {
        lua_pushinteger(L, message.data[i]);
        lua_rawseti(L, -2, i + 1);
    }
    lua_setfield(L, -2, "data");
    lua_pushlstring(L, (const char *)message.data, length);
    lua_setfield(L, -2, "payload");
    return 1;
}

static const char *state_name(twai_state_t state)
{
    switch (state) {
    case TWAI_STATE_STOPPED:
        return "stopped";
    case TWAI_STATE_RUNNING:
        return "running";
    case TWAI_STATE_BUS_OFF:
        return "bus_off";
    case TWAI_STATE_RECOVERING:
        return "recovering";
    default:
        return "unknown";
    }
}

static void set_status_integer(lua_State *L, const char *name, uint32_t value)
{
    lua_pushinteger(L, value);
    lua_setfield(L, -2, name);
}

static int can_status(lua_State *L)
{
    can_ud_t *ud = check_open(L, 1);
    twai_status_info_t status = {0};
    esp_err_t err = twai_get_status_info_v2(ud->handle, &status);
    if (err != ESP_OK) {
        return luaL_error(L, "can status failed: %s", esp_err_to_name(err));
    }

    lua_createtable(L, 0, 10);
    lua_pushstring(L, state_name(status.state));
    lua_setfield(L, -2, "state");
    set_status_integer(L, "msgs_to_tx", status.msgs_to_tx);
    set_status_integer(L, "msgs_to_rx", status.msgs_to_rx);
    set_status_integer(L, "tx_error_counter", status.tx_error_counter);
    set_status_integer(L, "rx_error_counter", status.rx_error_counter);
    set_status_integer(L, "tx_failed_count", status.tx_failed_count);
    set_status_integer(L, "rx_missed_count", status.rx_missed_count);
    set_status_integer(L, "rx_overrun_count", status.rx_overrun_count);
    set_status_integer(L, "arb_lost_count", status.arb_lost_count);
    set_status_integer(L, "bus_error_count", status.bus_error_count);
    return 1;
}

static int can_new(lua_State *L)
{
    lua_Integer tx_pin = luaL_checkinteger(L, 1);
    lua_Integer rx_pin = luaL_checkinteger(L, 2);
    lua_Integer bitrate = luaL_checkinteger(L, 3);
    twai_mode_t mode = TWAI_MODE_NORMAL;

    if (!GPIO_IS_VALID_OUTPUT_GPIO((gpio_num_t)tx_pin)) {
        return luaL_error(L, "can.new: invalid TX GPIO: %d", (int)tx_pin);
    }
    if (!GPIO_IS_VALID_GPIO((gpio_num_t)rx_pin)) {
        return luaL_error(L, "can.new: invalid RX GPIO: %d", (int)rx_pin);
    }
    if (!lua_isnoneornil(L, 4)) {
        const char *mode_value = luaL_checkstring(L, 4);
        if (strcmp(mode_value, "normal") == 0) {
            mode = TWAI_MODE_NORMAL;
        } else if (strcmp(mode_value, "listen_only") == 0) {
            mode = TWAI_MODE_LISTEN_ONLY;
        } else if (strcmp(mode_value, "no_ack") == 0) {
            mode = TWAI_MODE_NO_ACK;
        } else {
            return luaL_error(L, "can.new: mode must be normal, listen_only, or no_ack");
        }
    }

    twai_general_config_t general =
        TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)tx_pin, (gpio_num_t)rx_pin, mode);
    general.tx_queue_len = 10;
    general.rx_queue_len = 20;
    twai_timing_config_t timing = get_timing(L, bitrate);
    twai_filter_config_t filter = TWAI_FILTER_CONFIG_ACCEPT_ALL();
    twai_handle_t handle = NULL;

    esp_err_t err = twai_driver_install_v2(&general, &timing, &filter, &handle);
    if (err != ESP_OK) {
        return luaL_error(L, "can.new: driver install failed: %s", esp_err_to_name(err));
    }
    err = twai_start_v2(handle);
    if (err != ESP_OK) {
        (void)twai_driver_uninstall_v2(handle);
        return luaL_error(L, "can.new: driver start failed: %s", esp_err_to_name(err));
    }

    can_ud_t *ud = (can_ud_t *)lua_newuserdata(L, sizeof(*ud));
    *ud = (can_ud_t) {
        .handle = handle,
        .open = true,
    };
    luaL_getmetatable(L, CAN_METATABLE);
    lua_setmetatable(L, -2);
    return 1;
}

int luaopen_can(lua_State *L)
{
    if (luaL_newmetatable(L, CAN_METATABLE)) {
        lua_pushcfunction(L, can_gc);
        lua_setfield(L, -2, "__gc");
        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, can_transmit);
        lua_setfield(L, -2, "transmit");
        lua_pushcfunction(L, can_receive);
        lua_setfield(L, -2, "receive");
        lua_pushcfunction(L, can_status);
        lua_setfield(L, -2, "status");
        lua_pushcfunction(L, can_close);
        lua_setfield(L, -2, "close");
    }
    lua_pop(L, 1);

    lua_newtable(L);
    lua_pushcfunction(L, can_new);
    lua_setfield(L, -2, "new");
    return 1;
}

esp_err_t lua_module_can_register(void)
{
    return cap_lua_register_module("can", luaopen_can);
}
