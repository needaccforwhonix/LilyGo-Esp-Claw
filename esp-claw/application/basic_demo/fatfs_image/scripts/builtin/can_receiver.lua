-- ESP-Claw CAN receiver for LilyGo T-Connect-Pro + TD501MCANFD
-- Classical CAN/TWAI: TX=GPIO6, RX=GPIO7, 1 Mbps, standard ID 0xF1

local can = require("can")
local delay = require("delay")
local bm = require("board_manager")
local display = require("display")

local TX_PIN = 6
local RX_PIN = 7
local BITRATE = 1000000
local MESSAGE_ID = 0xF1
local RECEIVE_TIMEOUT_MS = 250

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
        display.fill_rect(0, 0, w, 48, 109, 40, 217)
    else
        display.fill_rect(0, 0, w, 48, 90, 100, 115)
    end
    display.draw_text_aligned(0, 0, w, 48, "CAN RECEIVER", {
        r = 255, g = 255, b = 255, font_size = 24,
        align = "center", valign = "middle"
    })
    display.draw_text_aligned(12, 68, w - 24, 38, status_text, {
        r = 235, g = 230, b = 255, font_size = 24,
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

local function bytes_to_hex(data)
    local parts = {}
    for i, value in ipairs(data) do
        parts[i] = string.format("%02X", value)
    end
    return table.concat(parts, " ")
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
    draw("Starting TWAI...", "TD501MCANFD", false)
    -- Normal mode is intentional: the receiver must send the CAN ACK bit.
    bus = can.new(TX_PIN, RX_PIN, BITRATE, "normal")
    draw("Waiting...", "No frame received", false)

    local count = 0
    local idle_ms = 0
    while true do
        local frame = bus:receive(RECEIVE_TIMEOUT_MS)
        if frame then
            idle_ms = 0
            if frame.id == MESSAGE_ID and not frame.extended then
                count = count + 1
                local hex = bytes_to_hex(frame.data)
                print(string.format("CAN RX #%d ID=0x%X DLC=%d data=%s",
                    count, frame.id, frame.dlc, hex))
                draw(string.format("RX #%d  ID=0x%X", count, frame.id),
                     hex, true)
            else
                print(string.format("CAN ignored ID=0x%X", frame.id))
            end
        else
            idle_ms = idle_ms + RECEIVE_TIMEOUT_MS
            if idle_ms == 3000 then
                local st = bus:status()
                draw("Waiting...", string.format("state=%s REC=%d",
                    st.state, st.rx_error_counter), false)
                print("CAN RX waiting: no matching frame for 3 seconds")
            end
        end
    end
end

local ok, err = xpcall(main, debug.traceback)
if not ok and display_ready then
    pcall(function() draw("CAN ERROR", tostring(err):sub(1, 42), false) end)
    delay.delay_ms(3000)
end
cleanup()
if not ok then error(err) end
