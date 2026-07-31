/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#include "lua_module_lora.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "cap_lua.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lauxlib.h"

#define LORA_METATABLE "lora.sx1262"
#define LORA_SPI_HOST SPI3_HOST
#define LORA_PIN_SCLK 12
#define LORA_PIN_MOSI 11
#define LORA_PIN_MISO 13
#define LORA_PIN_CS 14
#define LORA_PIN_RESET 42
#define LORA_PIN_BUSY 38
#define LORA_PIN_DIO1 45
#define LORA_SPI_HZ 2000000
#define LORA_MAX_PAYLOAD 255
#define LORA_BUSY_TIMEOUT_MS 2000
#define LORA_SPI_TIMEOUT_MS 5000
#define LORA_SPI_TRANSFER_MAX (LORA_MAX_PAYLOAD + 4)

#define SX_CMD_SET_STANDBY 0x80
#define SX_CMD_SET_RX 0x82
#define SX_CMD_SET_TX 0x83
#define SX_CMD_SET_RF_FREQUENCY 0x86
#define SX_CMD_SET_CAD_PARAMS 0x88
#define SX_CMD_CALIBRATE 0x89
#define SX_CMD_SET_PACKET_TYPE 0x8A
#define SX_CMD_SET_MODULATION_PARAMS 0x8B
#define SX_CMD_SET_PACKET_PARAMS 0x8C
#define SX_CMD_SET_TX_PARAMS 0x8E
#define SX_CMD_SET_BUFFER_BASE_ADDRESS 0x8F
#define SX_CMD_SET_RX_TX_FALLBACK_MODE 0x93
#define SX_CMD_SET_PA_CONFIG 0x95
#define SX_CMD_SET_REGULATOR_MODE 0x96
#define SX_CMD_CALIBRATE_IMAGE 0x98
#define SX_CMD_SET_DIO3_AS_TCXO_CTRL 0x97
#define SX_CMD_SET_DIO2_AS_RF_SWITCH_CTRL 0x9D
#define SX_CMD_GET_STATUS 0xC0
#define SX_CMD_SET_DIO_IRQ_PARAMS 0x08
#define SX_CMD_GET_IRQ_STATUS 0x12
#define SX_CMD_GET_RX_BUFFER_STATUS 0x13
#define SX_CMD_GET_PACKET_STATUS 0x14
#define SX_CMD_GET_DEVICE_ERRORS 0x17
#define SX_CMD_CLEAR_IRQ_STATUS 0x02
#define SX_CMD_CLEAR_DEVICE_ERRORS 0x07
#define SX_CMD_WRITE_REGISTER 0x0D
#define SX_CMD_READ_REGISTER 0x1D
#define SX_CMD_WRITE_BUFFER 0x0E
#define SX_CMD_READ_BUFFER 0x1E

#define SX_REG_LORA_SYNC_WORD_MSB 0x0740
#define SX_REG_IQ_CONFIG 0x0736
#define SX_REG_SENSITIVITY_CONFIG 0x0889
#define SX_REG_TX_CLAMP_CONFIG 0x08D8
#define SX_REG_OCP_CONFIGURATION 0x08E7
#define SX_REG_RX_GAIN 0x08AC

#define SX_IRQ_TX_DONE 0x0001
#define SX_IRQ_RX_DONE 0x0002
#define SX_IRQ_HEADER_ERR 0x0020
#define SX_IRQ_CRC_ERR 0x0040
#define SX_IRQ_TIMEOUT 0x0200
#define SX_IRQ_ALL 0x43FF

typedef struct {
    spi_device_handle_t spi;
    bool open;
    uint32_t frequency_hz;
    uint16_t bandwidth_khz;
    uint16_t preamble_length;
    uint8_t spreading_factor;
    uint8_t coding_rate;
    uint8_t sync_word;
    int8_t power_dbm;
    uint16_t last_irq;
    int16_t last_rssi;
    float last_snr;
    char last_error[128];
} lora_ud_t;

static bool s_lora_open;
static spi_transaction_t s_spi_transaction;
static uint8_t s_spi_tx[LORA_SPI_TRANSFER_MAX];
static uint8_t s_spi_rx[LORA_SPI_TRANSFER_MAX];
static bool s_spi_pending;

static esp_err_t radio_error(lora_ud_t *radio, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vsnprintf(radio->last_error, sizeof(radio->last_error), format, args);
    va_end(args);
    return ESP_FAIL;
}

static esp_err_t wait_busy(lora_ud_t *radio, uint32_t timeout_ms)
{
    TickType_t start = xTaskGetTickCount();
    TickType_t limit = pdMS_TO_TICKS(timeout_ms);
    while (gpio_get_level(LORA_PIN_BUSY)) {
        if ((xTaskGetTickCount() - start) >= limit) {
            return radio_error(radio, "BUSY timeout on GPIO%d", LORA_PIN_BUSY);
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    return ESP_OK;
}

static esp_err_t check_command_status(lora_ud_t *radio, uint8_t status, uint8_t opcode)
{
    uint8_t command = status & 0x0E;
    if (status == 0x00 || status == 0xFF) {
        return radio_error(radio, "SX1262 not responding (status=0x%02X, cmd=0x%02X)",
                           status, opcode);
    }
    if (command == 0x06 || command == 0x08 || command == 0x0A) {
        return radio_error(radio, "SX1262 command 0x%02X failed (status=0x%02X)",
                           opcode, status);
    }
    return ESP_OK;
}

static esp_err_t spi_transfer(lora_ud_t *radio, const uint8_t *tx, uint8_t *rx, size_t length)
{
    if (length > LORA_SPI_TRANSFER_MAX) {
        return radio_error(radio, "SPI transfer too large");
    }
    if (s_spi_pending) {
        return radio_error(radio, "previous SPI3 transaction is still pending");
    }

    esp_err_t err = wait_busy(radio, LORA_BUSY_TIMEOUT_MS);
    if (err != ESP_OK) {
        return err;
    }

    memcpy(s_spi_tx, tx, length);
    memset(s_spi_rx, 0, length);
    memset(&s_spi_transaction, 0, sizeof(s_spi_transaction));
    s_spi_transaction.length = length * 8;
    s_spi_transaction.tx_buffer = s_spi_tx;
    s_spi_transaction.rx_buffer = s_spi_rx;

    err = spi_device_queue_trans(radio->spi, &s_spi_transaction,
                                 pdMS_TO_TICKS(LORA_SPI_TIMEOUT_MS));
    if (err != ESP_OK) {
        return radio_error(radio, "SPI3 queue failed: %s", esp_err_to_name(err));
    }
    s_spi_pending = true;

    spi_transaction_t *completed = NULL;
    err = spi_device_get_trans_result(radio->spi, &completed,
                                      pdMS_TO_TICKS(LORA_SPI_TIMEOUT_MS));
    if (err != ESP_OK) {
        return radio_error(radio, "SPI3 transaction timeout: %s",
                           esp_err_to_name(err));
    }
    s_spi_pending = false;
    if (completed != &s_spi_transaction) {
        return radio_error(radio, "SPI3 returned wrong transaction");
    }
    memcpy(rx, s_spi_rx, length);
    return ESP_OK;
}

static esp_err_t write_command(lora_ud_t *radio, uint8_t opcode,
                               const uint8_t *data, size_t length)
{
    if (length > LORA_MAX_PAYLOAD) {
        return radio_error(radio, "command payload too large");
    }

    uint8_t tx[LORA_MAX_PAYLOAD + 1] = {0};
    uint8_t rx[LORA_MAX_PAYLOAD + 1] = {0};
    tx[0] = opcode;
    if (length) {
        memcpy(&tx[1], data, length);
    }
    esp_err_t err = spi_transfer(radio, tx, rx, length + 1);
    if (err != ESP_OK) {
        return err;
    }
    if (length) {
        err = check_command_status(radio, rx[1], opcode);
        if (err != ESP_OK) {
            return err;
        }
    }
    return wait_busy(radio, LORA_BUSY_TIMEOUT_MS);
}

static esp_err_t read_command(lora_ud_t *radio, uint8_t opcode, uint8_t *data, size_t length)
{
    if (length > LORA_MAX_PAYLOAD) {
        return radio_error(radio, "read payload too large");
    }

    uint8_t tx[LORA_MAX_PAYLOAD + 2] = {0};
    uint8_t rx[LORA_MAX_PAYLOAD + 2] = {0};
    tx[0] = opcode;
    esp_err_t err = spi_transfer(radio, tx, rx, length + 2);
    if (err != ESP_OK) {
        return err;
    }
    err = check_command_status(radio, rx[1], opcode);
    if (err != ESP_OK) {
        return err;
    }
    if (length) {
        memcpy(data, &rx[2], length);
    }
    return wait_busy(radio, LORA_BUSY_TIMEOUT_MS);
}

static esp_err_t write_register(lora_ud_t *radio, uint16_t address,
                                const uint8_t *data, size_t length)
{
    if (length > LORA_MAX_PAYLOAD - 2) {
        return radio_error(radio, "register write too large");
    }
    uint8_t tx[LORA_MAX_PAYLOAD + 3] = {0};
    uint8_t rx[LORA_MAX_PAYLOAD + 3] = {0};
    tx[0] = SX_CMD_WRITE_REGISTER;
    tx[1] = address >> 8;
    tx[2] = address & 0xFF;
    memcpy(&tx[3], data, length);
    esp_err_t err = spi_transfer(radio, tx, rx, length + 3);
    if (err != ESP_OK) {
        return err;
    }
    err = check_command_status(radio, rx[3], SX_CMD_WRITE_REGISTER);
    if (err != ESP_OK) {
        return err;
    }
    return wait_busy(radio, LORA_BUSY_TIMEOUT_MS);
}

static esp_err_t read_register(lora_ud_t *radio, uint16_t address,
                               uint8_t *data, size_t length)
{
    if (length > LORA_MAX_PAYLOAD - 3) {
        return radio_error(radio, "register read too large");
    }
    uint8_t tx[LORA_MAX_PAYLOAD + 4] = {0};
    uint8_t rx[LORA_MAX_PAYLOAD + 4] = {0};
    tx[0] = SX_CMD_READ_REGISTER;
    tx[1] = address >> 8;
    tx[2] = address & 0xFF;
    esp_err_t err = spi_transfer(radio, tx, rx, length + 4);
    if (err != ESP_OK) {
        return err;
    }
    err = check_command_status(radio, rx[3], SX_CMD_READ_REGISTER);
    if (err != ESP_OK) {
        return err;
    }
    memcpy(data, &rx[4], length);
    return wait_busy(radio, LORA_BUSY_TIMEOUT_MS);
}

static esp_err_t write_buffer(lora_ud_t *radio, const uint8_t *data, size_t length)
{
    if (length > LORA_MAX_PAYLOAD) {
        return radio_error(radio, "LoRa payload cannot exceed 255 bytes");
    }
    uint8_t tx[LORA_MAX_PAYLOAD + 2] = {0};
    uint8_t rx[LORA_MAX_PAYLOAD + 2] = {0};
    tx[0] = SX_CMD_WRITE_BUFFER;
    tx[1] = 0;
    memcpy(&tx[2], data, length);
    esp_err_t err = spi_transfer(radio, tx, rx, length + 2);
    if (err != ESP_OK) {
        return err;
    }
    err = check_command_status(radio, rx[2], SX_CMD_WRITE_BUFFER);
    if (err != ESP_OK) {
        return err;
    }
    return wait_busy(radio, LORA_BUSY_TIMEOUT_MS);
}

static esp_err_t read_buffer(lora_ud_t *radio, uint8_t offset, uint8_t *data, size_t length)
{
    uint8_t tx[LORA_MAX_PAYLOAD + 3] = {0};
    uint8_t rx[LORA_MAX_PAYLOAD + 3] = {0};
    tx[0] = SX_CMD_READ_BUFFER;
    tx[1] = offset;
    esp_err_t err = spi_transfer(radio, tx, rx, length + 3);
    if (err != ESP_OK) {
        return err;
    }
    err = check_command_status(radio, rx[2], SX_CMD_READ_BUFFER);
    if (err != ESP_OK) {
        return err;
    }
    memcpy(data, &rx[3], length);
    return wait_busy(radio, LORA_BUSY_TIMEOUT_MS);
}

static esp_err_t radio_reset(lora_ud_t *radio, int *busy_during_reset,
                             int *busy_after_reset)
{
    (void)radio;
    gpio_set_level(LORA_PIN_RESET, 1);
    esp_rom_delay_us(150);
    gpio_set_level(LORA_PIN_RESET, 0);
    esp_rom_delay_us(150);
    if (busy_during_reset) {
        *busy_during_reset = gpio_get_level(LORA_PIN_BUSY);
    }
    gpio_set_level(LORA_PIN_RESET, 1);
    esp_rom_delay_us(150);
    if (busy_after_reset) {
        *busy_after_reset = gpio_get_level(LORA_PIN_BUSY);
    }
    return ESP_OK;
}

static esp_err_t radio_wake(lora_ud_t *radio)
{
    /* NSS falling wakes SX1262. Hold it low until BUSY is released, exactly
     * like LilyGo's MicroPython SPItransfer(), before SPI owns CS14. */
    gpio_set_level(LORA_PIN_CS, 0);
    esp_err_t err = wait_busy(radio, LORA_BUSY_TIMEOUT_MS);
    gpio_set_level(LORA_PIN_CS, 1);
    return err;
}

static esp_err_t radio_standby(lora_ud_t *radio)
{
    const uint8_t mode = 0;
    return write_command(radio, SX_CMD_SET_STANDBY, &mode, 1);
}

static esp_err_t clear_irq(lora_ud_t *radio)
{
    const uint8_t data[2] = {0xFF, 0xFF};
    return write_command(radio, SX_CMD_CLEAR_IRQ_STATUS, data, sizeof(data));
}

static esp_err_t get_irq(lora_ud_t *radio, uint16_t *irq)
{
    uint8_t data[2] = {0};
    esp_err_t err = read_command(radio, SX_CMD_GET_IRQ_STATUS, data, sizeof(data));
    if (err == ESP_OK) {
        *irq = ((uint16_t)data[0] << 8) | data[1];
        radio->last_irq = *irq;
    }
    return err;
}

static esp_err_t get_radio_status(lora_ud_t *radio, uint8_t *status)
{
    const uint8_t tx[2] = {SX_CMD_GET_STATUS, 0};
    uint8_t rx[2] = {0};
    esp_err_t err = spi_transfer(radio, tx, rx, sizeof(tx));
    if (err != ESP_OK) {
        return err;
    }
    err = check_command_status(radio, rx[1], SX_CMD_GET_STATUS);
    if (err != ESP_OK) {
        return err;
    }
    *status = rx[1];
    return wait_busy(radio, LORA_BUSY_TIMEOUT_MS);
}

static esp_err_t set_irq(lora_ud_t *radio, uint16_t mask, uint16_t dio1)
{
    uint8_t data[8] = {
        mask >> 8, mask & 0xFF,
        dio1 >> 8, dio1 & 0xFF,
        0, 0, 0, 0,
    };
    return write_command(radio, SX_CMD_SET_DIO_IRQ_PARAMS, data, sizeof(data));
}

static uint8_t bandwidth_code(uint16_t bandwidth_khz)
{
    switch (bandwidth_khz) {
    case 125: return 0x04;
    case 250: return 0x05;
    case 500: return 0x06;
    default: return 0xFF;
    }
}

static esp_err_t set_packet_length(lora_ud_t *radio, uint8_t length)
{
    uint8_t params[6] = {
        radio->preamble_length >> 8,
        radio->preamble_length & 0xFF,
        0,
        length,
        1,
        0,
    };
    return write_command(radio, SX_CMD_SET_PACKET_PARAMS, params, sizeof(params));
}

static esp_err_t configure_modem(lora_ud_t *radio)
{
    uint8_t bw = bandwidth_code(radio->bandwidth_khz);
    uint32_t symbol_us = ((uint32_t)1 << radio->spreading_factor) * 1000U /
                         radio->bandwidth_khz;
    uint8_t modulation[4] = {
        radio->spreading_factor,
        bw,
        radio->coding_rate - 4,
        symbol_us >= 16000 ? 1 : 0,
    };
    esp_err_t err = write_command(radio, SX_CMD_SET_MODULATION_PARAMS,
                                  modulation, sizeof(modulation));
    if (err != ESP_OK) {
        return err;
    }
    return set_packet_length(radio, LORA_MAX_PAYLOAD);
}

static esp_err_t set_frequency(lora_ud_t *radio)
{
    uint8_t calibration[2] = {0};
    uint32_t mhz = radio->frequency_hz / 1000000U;
    if (mhz >= 902 && mhz <= 928) {
        calibration[0] = 0xE1;
        calibration[1] = 0xE9;
    } else if (mhz >= 863 && mhz <= 870) {
        calibration[0] = 0xD7;
        calibration[1] = 0xDB;
    } else if (mhz >= 779 && mhz <= 787) {
        calibration[0] = 0xC1;
        calibration[1] = 0xC5;
    } else if (mhz >= 470 && mhz <= 510) {
        calibration[0] = 0x75;
        calibration[1] = 0x81;
    } else if (mhz >= 430 && mhz <= 440) {
        calibration[0] = 0x6B;
        calibration[1] = 0x6F;
    } else {
        return radio_error(radio, "frequency %u Hz has no image calibration band",
                           (unsigned)radio->frequency_hz);
    }

    esp_err_t err = write_command(radio, SX_CMD_CALIBRATE_IMAGE,
                                  calibration, sizeof(calibration));
    if (err != ESP_OK) {
        return err;
    }
    uint32_t frf = (uint32_t)(((uint64_t)radio->frequency_hz << 25) / 32000000ULL);
    uint8_t frequency[4] = {frf >> 24, frf >> 16, frf >> 8, frf};
    return write_command(radio, SX_CMD_SET_RF_FREQUENCY, frequency, sizeof(frequency));
}

static esp_err_t radio_initialize(lora_ud_t *radio)
{
    /* Reclaim the radio pads. GPIO38 can retain a hold, sleep setting or a
     * previous GPIO-matrix route installed by another board component. A
     * MicroPython Pin(...) constructor resets that state implicitly; ESP-IDF
     * gpio_config() alone does not reliably detach every previous owner. */
    const gpio_num_t radio_pins[] = {
        LORA_PIN_RESET, LORA_PIN_CS, LORA_PIN_BUSY, LORA_PIN_DIO1,
    };
    for (size_t i = 0; i < sizeof(radio_pins) / sizeof(radio_pins[0]); ++i) {
        gpio_hold_dis(radio_pins[i]);
        gpio_sleep_sel_dis(radio_pins[i]);
        esp_err_t pin_err = gpio_reset_pin(radio_pins[i]);
        if (pin_err != ESP_OK) {
            return radio_error(radio, "GPIO%d reset failed: %s",
                               radio_pins[i], esp_err_to_name(pin_err));
        }
    }

    gpio_config_t output = {
        .pin_bit_mask = (1ULL << LORA_PIN_RESET) | (1ULL << LORA_PIN_CS),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t err = gpio_config(&output);
    if (err != ESP_OK) {
        return radio_error(radio, "RESET GPIO config failed: %s", esp_err_to_name(err));
    }
    gpio_config_t input = {
        .pin_bit_mask = (1ULL << LORA_PIN_BUSY) | (1ULL << LORA_PIN_DIO1),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    err = gpio_config(&input);
    if (err != ESP_OK) {
        return radio_error(radio, "BUSY/DIO1 GPIO config failed: %s", esp_err_to_name(err));
    }
    gpio_set_level(LORA_PIN_RESET, 1);
    gpio_set_level(LORA_PIN_CS, 1);

    spi_device_interface_config_t device = {
        .clock_speed_hz = LORA_SPI_HZ,
        .mode = 0,
        .spics_io_num = LORA_PIN_CS,
        .queue_size = 1,
    };

    int busy_before_reset = gpio_get_level(LORA_PIN_BUSY);
    int busy_during_reset = -1;
    int busy_after_reset = -1;
    err = radio_reset(radio, &busy_during_reset, &busy_after_reset);
    if (err != ESP_OK) {
        return err;
    }
    /* Keep the working 150 us reset pulse but give the radio digital core
     * enough time to settle before asserting NSS for the first command. */
    vTaskDelay(pdMS_TO_TICKS(10));
    err = radio_wake(radio);
    if (err != ESP_OK) {
        return radio_error(radio,
            "BUSY38 timeout levels pre=%d low=%d post=%d now=%d",
            busy_before_reset, busy_during_reset, busy_after_reset,
            gpio_get_level(LORA_PIN_BUSY));
    }

    err = spi_bus_add_device(LORA_SPI_HOST, &device, &radio->spi);
    if (err != ESP_OK) {
        return radio_error(radio,
            "add SX1262 to SPI3 failed: %s (LCD SPI bus must be initialized first)",
            esp_err_to_name(err));
    }

    /* Match LilyGo's reference driver: the first Standby response can be
     * 0x00 while the radio digital core is still becoming ready. */
    TickType_t standby_start = xTaskGetTickCount();
    TickType_t standby_limit = pdMS_TO_TICKS(3000);
    do {
        err = radio_standby(radio);
        if (err == ESP_OK) break;
        vTaskDelay(pdMS_TO_TICKS(10));
    } while ((xTaskGetTickCount() - standby_start) < standby_limit);
    if (err != ESP_OK) return err;

    const uint8_t tcxo[4] = {0x00, 0x00, 0x01, 0x40};
    err = write_command(radio, SX_CMD_SET_DIO3_AS_TCXO_CTRL, tcxo, sizeof(tcxo));
    if (err != ESP_OK) return err;

    const uint8_t buffer_base[2] = {0, 0};
    err = write_command(radio, SX_CMD_SET_BUFFER_BASE_ADDRESS,
                        buffer_base, sizeof(buffer_base));
    if (err != ESP_OK) return err;
    const uint8_t packet_type = 1;
    err = write_command(radio, SX_CMD_SET_PACKET_TYPE, &packet_type, 1);
    if (err != ESP_OK) return err;
    const uint8_t fallback = 0x20;
    err = write_command(radio, SX_CMD_SET_RX_TX_FALLBACK_MODE, &fallback, 1);
    if (err != ESP_OK) return err;

    const uint8_t cad[7] = {
        0x03, (uint8_t)(radio->spreading_factor + 13), 10, 0, 0, 0, 0
    };
    err = write_command(radio, SX_CMD_SET_CAD_PARAMS, cad, sizeof(cad));
    if (err != ESP_OK) return err;
    err = clear_irq(radio);
    if (err != ESP_OK) return err;
    err = set_irq(radio, 0, 0);
    if (err != ESP_OK) return err;

    const uint8_t calibrate = 0x7F;
    err = write_command(radio, SX_CMD_CALIBRATE, &calibrate, 1);
    if (err != ESP_OK) return err;
    vTaskDelay(pdMS_TO_TICKS(5));
    err = wait_busy(radio, LORA_BUSY_TIMEOUT_MS);
    if (err != ESP_OK) return err;

    const uint8_t regulator = 1;
    err = write_command(radio, SX_CMD_SET_REGULATOR_MODE, &regulator, 1);
    if (err != ESP_OK) return err;
    const uint8_t rf_switch = 1;
    err = write_command(radio, SX_CMD_SET_DIO2_AS_RF_SWITCH_CTRL, &rf_switch, 1);
    if (err != ESP_OK) return err;

    err = configure_modem(radio);
    if (err != ESP_OK) return err;
    err = set_frequency(radio);
    if (err != ESP_OK) return err;

    uint8_t sync[2] = {
        (uint8_t)((radio->sync_word & 0xF0) | 0x04),
        (uint8_t)((radio->sync_word << 4) | 0x04),
    };
    err = write_register(radio, SX_REG_LORA_SYNC_WORD_MSB, sync, sizeof(sync));
    if (err != ESP_OK) return err;

    uint8_t clamp = 0;
    err = read_register(radio, SX_REG_TX_CLAMP_CONFIG, &clamp, 1);
    if (err != ESP_OK) return err;
    clamp |= 0x1E;
    err = write_register(radio, SX_REG_TX_CLAMP_CONFIG, &clamp, 1);
    if (err != ESP_OK) return err;

    const uint8_t pa[4] = {0x04, 0x07, 0x00, 0x01};
    err = write_command(radio, SX_CMD_SET_PA_CONFIG, pa, sizeof(pa));
    if (err != ESP_OK) return err;
    uint8_t tx_params[2] = {(uint8_t)radio->power_dbm, 0x04};
    err = write_command(radio, SX_CMD_SET_TX_PARAMS, tx_params, sizeof(tx_params));
    if (err != ESP_OK) return err;

    const uint8_t ocp = 24;
    err = write_register(radio, SX_REG_OCP_CONFIGURATION, &ocp, 1);
    if (err != ESP_OK) return err;

    uint8_t sensitivity = 0;
    err = read_register(radio, SX_REG_SENSITIVITY_CONFIG, &sensitivity, 1);
    if (err != ESP_OK) return err;
    if (radio->bandwidth_khz == 500) sensitivity &= 0xFB;
    else sensitivity |= 0x04;
    err = write_register(radio, SX_REG_SENSITIVITY_CONFIG, &sensitivity, 1);
    if (err != ESP_OK) return err;

    uint8_t iq = 0;
    err = read_register(radio, SX_REG_IQ_CONFIG, &iq, 1);
    if (err != ESP_OK) return err;
    iq &= (uint8_t)~0x04;
    err = write_register(radio, SX_REG_IQ_CONFIG, &iq, 1);
    if (err != ESP_OK) return err;

    const uint8_t rx_gain = 0x96;
    err = write_register(radio, SX_REG_RX_GAIN, &rx_gain, 1);
    if (err != ESP_OK) return err;

    const uint8_t clear_errors[2] = {0, 0};
    err = write_command(radio, SX_CMD_CLEAR_DEVICE_ERRORS,
                        clear_errors, sizeof(clear_errors));
    if (err != ESP_OK) return err;
    err = clear_irq(radio);
    if (err != ESP_OK) return err;

    radio->last_error[0] = 0;
    return ESP_OK;
}
static esp_err_t radio_transmit_packet(lora_ud_t *radio, const uint8_t *data,
                                       size_t length, uint32_t timeout_ms)
{
    if (length == 0 || length > LORA_MAX_PAYLOAD) {
        return radio_error(radio, "payload length must be 1..255 bytes");
    }
    esp_err_t err = radio_standby(radio);
    if (err != ESP_OK) return err;
    const uint8_t buffer_base[2] = {0, 0};
    err = write_command(radio, SX_CMD_SET_BUFFER_BASE_ADDRESS,
                        buffer_base, sizeof(buffer_base));
    if (err != ESP_OK) return err;
    err = set_packet_length(radio, (uint8_t)length);
    if (err != ESP_OK) return err;
    err = set_irq(radio, SX_IRQ_TX_DONE | SX_IRQ_TIMEOUT, SX_IRQ_TX_DONE);
    if (err != ESP_OK) return err;
    err = write_buffer(radio, data, length);
    if (err != ESP_OK) return err;
    err = clear_irq(radio);
    if (err != ESP_OK) return err;

    const uint8_t tx_timeout[3] = {0, 0, 0};
    err = write_command(radio, SX_CMD_SET_TX, tx_timeout, sizeof(tx_timeout));
    if (err != ESP_OK) return err;

    TickType_t start = xTaskGetTickCount();
    TickType_t limit = pdMS_TO_TICKS(timeout_ms);
    while ((xTaskGetTickCount() - start) < limit) {
        uint16_t irq = 0;
        err = get_irq(radio, &irq);
        if (err != ESP_OK) return err;
        if (irq & SX_IRQ_TX_DONE) {
            clear_irq(radio);
            radio_standby(radio);
            radio->last_error[0] = 0;
            return ESP_OK;
        }
        if (irq & SX_IRQ_TIMEOUT) {
            clear_irq(radio);
            radio_standby(radio);
            return radio_error(radio, "SX1262 TX IRQ timeout (irq=0x%04X)", irq);
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }

    radio_standby(radio);
    clear_irq(radio);
    return radio_error(radio, "SX1262 TX host timeout after %u ms",
                       (unsigned)timeout_ms);
}

static esp_err_t radio_receive_packet(lora_ud_t *radio, uint8_t *data,
                                      size_t *length, uint32_t timeout_ms)
{
    esp_err_t err = radio_standby(radio);
    if (err != ESP_OK) return err;
    const uint8_t buffer_base[2] = {0, 0};
    err = write_command(radio, SX_CMD_SET_BUFFER_BASE_ADDRESS,
                        buffer_base, sizeof(buffer_base));
    if (err != ESP_OK) return err;
    err = set_packet_length(radio, LORA_MAX_PAYLOAD);
    if (err != ESP_OK) return err;
    uint16_t mask = SX_IRQ_RX_DONE | SX_IRQ_CRC_ERR |
                    SX_IRQ_HEADER_ERR | SX_IRQ_TIMEOUT;
    err = set_irq(radio, mask, SX_IRQ_RX_DONE);
    if (err != ESP_OK) return err;
    err = clear_irq(radio);
    if (err != ESP_OK) return err;

    const uint8_t rx_timeout[3] = {0xFF, 0xFF, 0xFF};
    err = write_command(radio, SX_CMD_SET_RX, rx_timeout, sizeof(rx_timeout));
    if (err != ESP_OK) return err;

    TickType_t start = xTaskGetTickCount();
    TickType_t limit = pdMS_TO_TICKS(timeout_ms);
    do {
        uint16_t irq = 0;
        err = get_irq(radio, &irq);
        if (err != ESP_OK) return err;
        if (irq & (SX_IRQ_CRC_ERR | SX_IRQ_HEADER_ERR)) {
            clear_irq(radio);
            radio_standby(radio);
            return radio_error(radio, "LoRa RX CRC/header error (irq=0x%04X)", irq);
        }
        if (irq & SX_IRQ_RX_DONE) {
            uint8_t buffer_status[2] = {0};
            err = read_command(radio, SX_CMD_GET_RX_BUFFER_STATUS,
                               buffer_status, sizeof(buffer_status));
            if (err != ESP_OK) return err;
            *length = buffer_status[0];
            err = read_buffer(radio, buffer_status[1], data, *length);
            if (err != ESP_OK) return err;

            uint8_t packet_status[3] = {0};
            err = read_command(radio, SX_CMD_GET_PACKET_STATUS,
                               packet_status, sizeof(packet_status));
            if (err != ESP_OK) return err;
            radio->last_rssi = -(int16_t)(packet_status[0] / 2);
            radio->last_snr = (float)(int8_t)packet_status[1] / 4.0f;
            clear_irq(radio);
            radio_standby(radio);
            radio->last_error[0] = 0;
            return ESP_OK;
        }
        if (irq & SX_IRQ_TIMEOUT) {
            clear_irq(radio);
            radio_standby(radio);
            return ESP_ERR_TIMEOUT;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    } while ((xTaskGetTickCount() - start) < limit);

    radio_standby(radio);
    clear_irq(radio);
    return ESP_ERR_TIMEOUT;
}

static void close_radio(lora_ud_t *radio)
{
    if (!radio->open) return;
    (void)radio_standby(radio);
    if (radio->spi) {
        (void)spi_bus_remove_device(radio->spi);
        radio->spi = NULL;
    }
    radio->open = false;
    s_lora_open = false;
}

static lora_ud_t *check_radio(lua_State *L)
{
    lora_ud_t *radio = (lora_ud_t *)luaL_checkudata(L, 1, LORA_METATABLE);
    if (!radio->open || !radio->spi) {
        luaL_error(L, "lora: radio is closed");
    }
    return radio;
}

static lua_Integer option_integer(lua_State *L, int table_index,
                                  const char *field, lua_Integer default_value)
{
    lua_getfield(L, table_index, field);
    lua_Integer value = lua_isnil(L, -1)
                            ? default_value : luaL_checkinteger(L, -1);
    lua_pop(L, 1);
    return value;
}

static int lora_close(lua_State *L)
{
    lora_ud_t *radio = (lora_ud_t *)luaL_checkudata(L, 1, LORA_METATABLE);
    close_radio(radio);
    return 0;
}

static int lora_gc(lua_State *L)
{
    lora_ud_t *radio = (lora_ud_t *)luaL_testudata(L, 1, LORA_METATABLE);
    if (radio) close_radio(radio);
    return 0;
}

static int lora_transmit(lua_State *L)
{
    lora_ud_t *radio = check_radio(L);
    size_t length = 0;
    const char *payload = luaL_checklstring(L, 2, &length);
    lua_Integer timeout = luaL_optinteger(L, 3, 15000);
    if (timeout <= 0 || (uint64_t)timeout > UINT32_MAX) {
        return luaL_error(L, "lora transmit: timeout must be 1..%u ms", UINT32_MAX);
    }
    esp_err_t err = radio_transmit_packet(radio, (const uint8_t *)payload,
                                          length, (uint32_t)timeout);
    if (err != ESP_OK) {
        return luaL_error(L, "lora transmit failed: %s", radio->last_error);
    }
    lua_pushboolean(L, true);
    return 1;
}

static int lora_receive(lua_State *L)
{
    lora_ud_t *radio = check_radio(L);
    lua_Integer timeout = luaL_optinteger(L, 2, 1000);
    if (timeout < 0 || (uint64_t)timeout > UINT32_MAX) {
        return luaL_error(L, "lora receive: invalid timeout");
    }

    uint8_t payload[LORA_MAX_PAYLOAD] = {0};
    size_t length = 0;
    esp_err_t err = radio_receive_packet(radio, payload, &length,
                                          (uint32_t)timeout);
    if (err == ESP_ERR_TIMEOUT) {
        lua_pushnil(L);
        return 1;
    }
    if (err != ESP_OK) {
        return luaL_error(L, "lora receive failed: %s", radio->last_error);
    }

    lua_createtable(L, 0, 5);
    lua_pushlstring(L, (const char *)payload, length);
    lua_setfield(L, -2, "data");
    lua_pushinteger(L, length);
    lua_setfield(L, -2, "length");
    lua_pushinteger(L, radio->last_rssi);
    lua_setfield(L, -2, "rssi");
    lua_pushnumber(L, radio->last_snr);
    lua_setfield(L, -2, "snr");
    lua_pushinteger(L, radio->last_irq);
    lua_setfield(L, -2, "irq");
    return 1;
}

static int lora_status(lua_State *L)
{
    lora_ud_t *radio = check_radio(L);
    uint8_t status = 0;
    uint8_t errors[2] = {0};
    esp_err_t err = get_radio_status(radio, &status);
    if (err != ESP_OK) {
        return luaL_error(L, "lora status failed: %s", radio->last_error);
    }
    err = read_command(radio, SX_CMD_GET_DEVICE_ERRORS, errors, sizeof(errors));
    if (err != ESP_OK) {
        return luaL_error(L, "lora status failed: %s", radio->last_error);
    }

    lua_createtable(L, 0, 13);
    lua_pushstring(L, "SX1262");
    lua_setfield(L, -2, "chip");
    lua_pushinteger(L, status);
    lua_setfield(L, -2, "raw_status");
    lua_pushinteger(L, ((uint16_t)errors[0] << 8) | errors[1]);
    lua_setfield(L, -2, "device_errors");
    lua_pushinteger(L, radio->frequency_hz);
    lua_setfield(L, -2, "frequency_hz");
    lua_pushinteger(L, radio->bandwidth_khz);
    lua_setfield(L, -2, "bandwidth_khz");
    lua_pushinteger(L, radio->spreading_factor);
    lua_setfield(L, -2, "spreading_factor");
    lua_pushinteger(L, radio->coding_rate);
    lua_setfield(L, -2, "coding_rate");
    lua_pushinteger(L, radio->power_dbm);
    lua_setfield(L, -2, "power_dbm");
    lua_pushinteger(L, radio->last_irq);
    lua_setfield(L, -2, "last_irq");
    lua_pushinteger(L, radio->last_rssi);
    lua_setfield(L, -2, "last_rssi");
    lua_pushnumber(L, radio->last_snr);
    lua_setfield(L, -2, "last_snr");
    lua_pushstring(L, radio->last_error);
    lua_setfield(L, -2, "last_error");
    return 1;
}

static int lora_new(lua_State *L)
{
    if (s_lora_open) {
        return luaL_error(L, "lora.new: SX1262 is already open");
    }
    if (!lua_isnoneornil(L, 1)) {
        luaL_checktype(L, 1, LUA_TTABLE);
    } else {
        lua_newtable(L);
        lua_replace(L, 1);
    }

    lua_Integer frequency = option_integer(L, 1, "frequency_hz", 923000000);
    lua_Integer bandwidth = option_integer(L, 1, "bandwidth_khz", 500);
    lua_Integer sf = option_integer(L, 1, "spreading_factor", 12);
    lua_Integer cr = option_integer(L, 1, "coding_rate", 7);
    lua_Integer power = option_integer(L, 1, "power_dbm", 14);
    lua_Integer preamble = option_integer(L, 1, "preamble_length", 8);
    lua_Integer sync_word = option_integer(L, 1, "sync_word", 0x12);

    if (frequency < 150000000 || frequency > 960000000) {
        return luaL_error(L, "lora.new: frequency_hz must be 150000000..960000000");
    }
    if (bandwidth_code((uint16_t)bandwidth) == 0xFF) {
        return luaL_error(L, "lora.new: bandwidth_khz must be 125, 250, or 500");
    }
    if (sf < 5 || sf > 12) {
        return luaL_error(L, "lora.new: spreading_factor must be 5..12");
    }
    if (cr < 5 || cr > 8) {
        return luaL_error(L, "lora.new: coding_rate must be 5..8");
    }
    if (power < -9 || power > 22) {
        return luaL_error(L, "lora.new: power_dbm must be -9..22");
    }
    if (preamble < 1 || preamble > UINT16_MAX) {
        return luaL_error(L, "lora.new: invalid preamble_length");
    }
    if (sync_word < 0 || sync_word > UINT8_MAX) {
        return luaL_error(L, "lora.new: sync_word must be 0..255");
    }

    lora_ud_t *radio = (lora_ud_t *)lua_newuserdata(L, sizeof(*radio));
    memset(radio, 0, sizeof(*radio));
    radio->frequency_hz = (uint32_t)frequency;
    radio->bandwidth_khz = (uint16_t)bandwidth;
    radio->spreading_factor = (uint8_t)sf;
    radio->coding_rate = (uint8_t)cr;
    radio->power_dbm = (int8_t)power;
    radio->preamble_length = (uint16_t)preamble;
    radio->sync_word = (uint8_t)sync_word;

    esp_err_t err = radio_initialize(radio);
    if (err != ESP_OK) {
        if (radio->spi && !s_spi_pending) {
            (void)spi_bus_remove_device(radio->spi);
            radio->spi = NULL;
        }
        return luaL_error(L, "lora.new failed: %s", radio->last_error);
    }
    radio->open = true;
    s_lora_open = true;
    luaL_getmetatable(L, LORA_METATABLE);
    lua_setmetatable(L, -2);
    return 1;
}

int luaopen_lora(lua_State *L)
{
    if (luaL_newmetatable(L, LORA_METATABLE)) {
        lua_pushcfunction(L, lora_gc);
        lua_setfield(L, -2, "__gc");
        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, lora_transmit);
        lua_setfield(L, -2, "transmit");
        lua_pushcfunction(L, lora_receive);
        lua_setfield(L, -2, "receive");
        lua_pushcfunction(L, lora_status);
        lua_setfield(L, -2, "status");
        lua_pushcfunction(L, lora_close);
        lua_setfield(L, -2, "close");
    }
    lua_pop(L, 1);

    lua_newtable(L);
    lua_pushcfunction(L, lora_new);
    lua_setfield(L, -2, "new");
    lua_pushinteger(L, LORA_PIN_CS);
    lua_setfield(L, -2, "CS_PIN");
    lua_pushinteger(L, LORA_PIN_RESET);
    lua_setfield(L, -2, "RESET_PIN");
    lua_pushinteger(L, LORA_PIN_BUSY);
    lua_setfield(L, -2, "BUSY_PIN");
    lua_pushinteger(L, LORA_PIN_DIO1);
    lua_setfield(L, -2, "DIO1_PIN");
    return 1;
}

esp_err_t lua_module_lora_register(void)
{
    return cap_lua_register_module("lora", luaopen_lora);
}
