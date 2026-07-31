-- ESP-Claw RS485 receiver
-- Hardware: ESP32-S3 + TD501D485H-A
-- UART1: TX = GPIO17, RX = GPIO18, 115200 8N1

local uart = require("uart")
local delay = require("delay")
local board_manager = require("board_manager")
local display = require("display")

local UART_PORT = 1
local UART_TX = 17
local UART_RX = 18
local BAUD_RATE = 115200
local POLL_INTERVAL_MS = 20
local MAX_LINE_LENGTH = 128

local serial = nil
local display_ready = false

local function to_ascii(text)
    return (text:gsub(".", function(ch)
        local value = string.byte(ch)
        if value >= 32 and value <= 126 then
            return ch
        end
        return "?"
    end))
end

local function draw_screen(title, line1, line2, accent)
    if not display_ready then
        return
    end

    display.begin_frame({ clear = true, color = "#08111f" })
    display.fill_rect(0, 0, display.width, 42, accent or "#7c3aed")
    display.draw_text_aligned(0, 0, display.width, 42, title, {
        color = "white",
        font_size = 24,
        align = "center",
        valign = "middle",
    })
    display.draw_text_aligned(8, 58, display.width - 16, 36, line1 or "", {
        color = "#fef3c7",
        font_size = 24,
        align = "center",
        valign = "middle",
    })
    display.draw_text_aligned(8, 102, display.width - 16, 32, line2 or "", {
        color = "#ddd6fe",
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
    print("RS485 receiver is preparing")

    init_display()
    serial = uart.new(UART_PORT, UART_TX, UART_RX, BAUD_RATE)
    serial:flush_input()

    for n = 3, 1, -1 do
        print(n)
        draw_screen("RS485 RECEIVER", "Preparing: " .. n, "TD501D485H-A", "#6d28d9")
        delay.delay_ms(1000)
    end

    print("RS485 preparation completed")
    draw_screen("RS485 RECEIVER", "Waiting...", "No data yet", "#6d28d9")

    local received_count = 0

    while true do
        while serial:available() > 0 do
            local line = serial:read_line(MAX_LINE_LENGTH, 200)

            if line and #line > 0 then
                local clean = line:gsub("[\r\n]+$", "")

                if #clean > 0 then
                    received_count = received_count + 1
                    local screen_text = to_ascii(clean):sub(1, 32)

                    print("RS485 receive data: " .. clean)
                    draw_screen(
                        "RS485 RECEIVER",
                        "Data: " .. screen_text,
                        "Packets: " .. received_count,
                        "#6d28d9"
                    )
                end
            end
        end

        delay.delay_ms(POLL_INTERVAL_MS)
    end
end

local ok, err = xpcall(main, function(message)
    return tostring(message)
end)

cleanup()

if not ok then
    print("RS485 receiver stopped: " .. err)
    error(err)
end
