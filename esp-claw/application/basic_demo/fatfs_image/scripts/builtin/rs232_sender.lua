-- ESP-Claw RS485 sender
-- Hardware: ESP32-S3 + TD501D485H-A
-- UART1: TX = GPIO17, RX = GPIO18, 115200 8N1

local uart = require("uart")
local delay = require("delay")
local board_manager = require("board_manager")
local display = require("display")

local UART_PORT = 1
local UART_TX = 4
local UART_RX = 5
local BAUD_RATE = 115200
local SEND_INTERVAL_MS = 3000

local serial = nil
local display_ready = false

local function draw_screen(title, line1, line2, accent)
    if not display_ready then
        return
    end

    display.begin_frame({ clear = true, color = "#08111f" })
    display.fill_rect(0, 0, display.width, 42, accent or "#168aad")
    display.draw_text_aligned(0, 0, display.width, 42, title, {
        color = "white",
        font_size = 24,
        align = "center",
        valign = "middle",
    })
    display.draw_text_aligned(8, 58, display.width - 16, 36, line1 or "", {
        color = "#d8f3dc",
        font_size = 24,
        align = "center",
        valign = "middle",
    })
    display.draw_text_aligned(8, 102, display.width - 16, 32, line2 or "", {
        color = "#90e0ef",
        font_size = 18,
        align = "center",
        valign = "middle",
    })
    display.draw_text_aligned(0, display.height - 28, display.width, 20,
        "UART1  TX17 RX18  115200", {
            color = "#8d99ae",
            font_size = 14,
            align = "center",
            valign = "middle",
        })
    display.present()
    display.end_frame()
end

local function init_display()
    local panel_handle, io_handle, width, height, panel_if, pixel_format =
        board_manager.get_display_lcd_params("display_lcd")

    display.init(panel_handle, io_handle, width, height, panel_if, pixel_format)
    display_ready = true
    display.backlight(true)
end

local function cleanup()
    if serial then
        pcall(function() serial:close() end)
        serial = nil
    end

    if display_ready then
        pcall(function()
            if display.frame_active() then
                display.end_frame()
            end
        end)
        pcall(display.deinit)
        display_ready = false
    end
end

local function main()
    print("Ciallo")
    print("RS485 sender is preparing")

    init_display()
    serial = uart.new(UART_PORT, UART_TX, UART_RX, BAUD_RATE)
    serial:flush_input()

    for n = 3, 1, -1 do
        print(n)
        draw_screen("RS485 SENDER", "Preparing: " .. n, "TD501D485H-A", "#1d4ed8")
        delay.delay_ms(1000)
    end

    print("RS485 preparation completed")
    draw_screen("RS485 SENDER", "Ready", "Sending every 3 seconds", "#047857")
    delay.delay_ms(500)

    local counter = 0
    while true do
        counter = counter + 1
        local payload = string.format("%d\n", counter)
        local bytes_sent = serial:write(payload)

        print(string.format("RS485 send data: %d (%d bytes)", counter, bytes_sent))
        draw_screen(
            "RS485 SENDER",
            "Sent: " .. counter,
            "Bytes: " .. bytes_sent,
            "#047857"
        )

        delay.delay_ms(SEND_INTERVAL_MS)
    end
end

local ok, err = xpcall(main, function(message)
    return tostring(message)
end)

cleanup()

if not ok then
    print("RS485 sender stopped: " .. err)
    error(err)
end
