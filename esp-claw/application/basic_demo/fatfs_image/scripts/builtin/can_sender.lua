-- ESP-Claw CAN sender for LilyGo T-Connect-Pro + TD501MCANFD
-- Classical CAN/TWAI: TX=GPIO6, RX=GPIO7, 1 Mbps, standard ID 0xF1

local can = require("can")
local delay = require("delay")
local bm = require("board_manager")
local display = require("display")

local TX_PIN = 6
local RX_PIN = 7
local BITRATE = 1000000
local MESSAGE_ID = 0xF1
local DATA = {1, 2, 3, 4, 5, 6, 7, 8}
local SEND_INTERVAL_MS = 1000

local bus
local display_ready = false

local function init_display()
    local panel, io, width, height, panel_if =
        bm.get_display_lcd_params("display_lcd")
    assert(panel, "display_lcd is unavailable: " .. tostring(io))
    display.init(panel, io, width, height, panel_if)
    display_ready = true
    display.backlight(true)
end

local function draw(status_text, detail, good)
    if not display_ready then return end
    local w, h = display.width(), display.height()
    display.begin_frame({clear = true, r = 8, g = 17, b = 31})
    if good then
        display.fill_rect(0, 0, w, 48, 4, 120, 87)
    else
        display.fill_rect(0, 0, w, 48, 180, 55, 55)
    end
    display.draw_text_aligned(0, 0, w, 48, "CAN SENDER", {
        r = 255, g = 255, b = 255, font_size = 24,
        align = "center", valign = "middle"
    })
    display.draw_text_aligned(12, 68, w - 24, 38, status_text, {
        r = 220, g = 245, b = 255, font_size = 24,
        align = "center", valign = "middle"
    })
    display.draw_text_aligned(12, 116, w - 24, 32, detail, {
        r = 125, g = 220, b = 255, font_size = 18,
        align = "center", valign = "middle"
    })
    display.draw_text_aligned(0, h - 34, w, 24,
        "ID 0xF1 | TX6 RX7 | 1 Mbps", {
            r = 150, g = 165, b = 185, font_size = 16,
            align = "center", valign = "middle"
        })
    display.present()
    display.end_frame()
end

local function cleanup()
    if bus then
        pcall(function() bus:close() end)
        bus = nil
    end
    if display_ready then
        pcall(function()
            if display.frame_active() then display.end_frame() end
        end)
        pcall(display.deinit)
        display_ready = false
    end
end

local function main()
    init_display()
    draw("Starting TWAI...", "TD501MCANFD", true)
    bus = can.new(TX_PIN, RX_PIN, BITRATE, "normal")
    delay.delay_ms(300)

    local count = 0
    while true do
        count = count + 1
        local ok, err = pcall(function()
            bus:transmit(MESSAGE_ID, DATA, {timeout_ms = 200})
        end)
        delay.delay_ms(50)

        local st = bus:status()
        local healthy = ok and st.state == "running"
            and st.tx_error_counter == 0 and st.bus_error_count == 0
        local line1
        if ok then
            line1 = string.format("Sent #%d", count)
        else
            line1 = "TX ERROR"
        end
        local line2 = string.format("state=%s TEC=%d busErr=%d",
            st.state, st.tx_error_counter, st.bus_error_count)
        print(string.format(
            "CAN TX #%d ID=0x%X data=01 02 03 04 05 06 07 08 state=%s TEC=%d busErr=%d%s",
            count, MESSAGE_ID, st.state, st.tx_error_counter,
            st.bus_error_count, ok and "" or (" error=" .. tostring(err))))
        draw(line1, line2, healthy)
        delay.delay_ms(SEND_INTERVAL_MS - 50)
    end
end

local ok, err = xpcall(main, debug.traceback)
if not ok and display_ready then
    pcall(function() draw("CAN ERROR", tostring(err):sub(1, 42), false) end)
    delay.delay_ms(3000)
end
cleanup()
if not ok then error(err) end
