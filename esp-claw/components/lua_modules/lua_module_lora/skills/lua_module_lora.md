# Lua SX1262 LoRa

Use `lora` for the onboard SX1262 on LilyGo T-Connect-Pro. The native module shares the LCD SPI3 bus safely through ESP-IDF. Hardware pins are SCK12, MOSI11, MISO13, CS14, RESET42, BUSY38, and DIO1 45.

## Open the radio

```lua
local lora = require("lora")
local radio = lora.new({
    frequency_hz = 923000000,
    bandwidth_khz = 500,
    spreading_factor = 12,
    coding_rate = 8,
    power_dbm = -5,
    preamble_length = 8,
    sync_word = 0x12,
})
```

Both nodes must use identical frequency, bandwidth, spreading factor, coding rate, preamble, sync word, CRC and IQ settings. Attach a suitable antenna before transmitting. Confirm that the selected frequency is legal in the deployment region.

## Send and receive

```lua
radio:transmit("hello", 15000)
local packet = radio:receive(1000)
if packet then
    print(packet.data, packet.rssi, packet.snr)
end
```

`transmit(data, timeout_ms)` sends 1..255 binary bytes. `receive(timeout_ms)` returns nil on host timeout, or a table containing `data`, `length`, `rssi`, `snr`, and `irq`. `status()` reports chip configuration and SX1262 device errors. Always call `close()` when finished. Only one LoRa handle can be open at a time.

For the built-in long-running demos, use `lua_run_script_async` with `exclusive="display"` and `timeout_ms=0`: `builtin/lora_sender.lua` on one device and `builtin/lora_receiver.lua` on the other.
