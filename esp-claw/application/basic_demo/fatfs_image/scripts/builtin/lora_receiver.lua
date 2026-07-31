-- ESP-Claw SX1262 LoRa receiver for LilyGo T-Connect-Pro
local delay = require("delay")
local bm = require("board_manager")
local display = require("display")

local lora
local radio
local display_ready=false

local function init_display()
    local panel, io, width, height, panel_if =
        bm.get_display_lcd_params("display_lcd")
    assert(panel, "display unavailable: " .. tostring(io))
    display.init(panel, io, width, height, panel_if)
    display_ready=true
    display.backlight(true)
    assert(display.width() > 0 and display.height() > 0,
        "display initialized with invalid size")
end

local function ascii(text)
    return (text:gsub(".", function(ch)
        local b=string.byte(ch)
        return (b >= 32 and b <= 126) and ch or "."
    end))
end

local function draw(title, line1, line2, good)
    if not display_ready then return end
    local w, h=display.width(), display.height()
    display.begin_frame({clear=true, r=7, g=15, b=27})
    if good then display.fill_rect(0, 0, w, 48, 105, 45, 205)
    else display.fill_rect(0, 0, w, 48, 90, 100, 115) end
    display.draw_text_aligned(0, 0, w, 48, title, {
        r=255, g=255, b=255, font_size=24,
        align="center", valign="middle"
    })
    display.draw_text_aligned(10, 66, w-20, 38, line1 or "", {
        r=235, g=230, b=255, font_size=22,
        align="center", valign="middle"
    })
    display.draw_text_aligned(10, 112, w-20, 48, line2 or "", {
        r=130, g=215, b=255, font_size=16,
        align="center", valign="middle"
    })
    display.draw_text_aligned(0, h-32, w, 22,
        "SX1262 868.6MHz SF12 BW125", {
            r=150, g=165, b=185, font_size=16,
            align="center", valign="middle"
        })
    display.present()
    display.end_frame()
end

local function cleanup()
    if radio then pcall(function() radio:close() end); radio=nil end
    if display_ready then
        pcall(function()
            if display.frame_active() then display.end_frame() end
        end)
        pcall(display.deinit)
        display_ready=false
    end
end

local function main()
    -- Stop emote submissions before taking the shared SPI3 bus.
    init_display()
    draw("LORA RECEIVER", "Initializing radio...",
         "SPI3 CS14 BUSY38", false)
    delay.delay_ms(300)
    print("LORA_STAGE: loading module")
    lora=require("lora")
    print("LORA_STAGE: initializing SX1262")
    radio=lora.new({
        frequency_hz=868600000,
        bandwidth_khz=125,
        spreading_factor=12,
        coding_rate=8,
        power_dbm=22,
        preamble_length=8,
        sync_word=0x12,
    })
    print("LORA_STAGE: SX1262 ready")
    draw("LORA RECEIVER", "Waiting...", "No packet yet", false)

    local count=0
    while true do
        local packet=radio:receive(1000)
        if packet then
            count=count+1
            local text=ascii(packet.data):sub(1, 38)
            print(string.format("LoRa RX #%d RSSI=%d SNR=%.2f: %s",
                count, packet.rssi, packet.snr, text))
            draw("LORA RECEIVER", string.format("RX #%d  RSSI %d", count, packet.rssi),
                 string.format("SNR %.2f  %s", packet.snr, text), true)
        end
    end
end

local ok, err=xpcall(main, debug.traceback)
if not ok then
    print("LoRa receiver error: " .. tostring(err))
    local cause=tostring(err):match("lora%.new failed:%s*(.+)")
        or tostring(err):match("lora receive failed:%s*(.+)")
        or tostring(err)
    if not display_ready then pcall(init_display) end
    pcall(function()
        draw("LORA ERROR", "Initialization/RX failed",
             cause:gsub("[\r\n]+", " "):sub(1, 48), false)
    end)
    -- Keep the failure visible until Clawbot stops/replaces this async job.
    while true do delay.delay_ms(1000) end
end
cleanup()
if not ok then error(err) end
