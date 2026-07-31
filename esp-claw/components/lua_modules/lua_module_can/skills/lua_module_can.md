# Lua CAN/TWAI

Use the `can` module for classical CAN 2.0 through ESP32 TWAI and an external transceiver. LilyGo T-Connect-Pro with TD501MCANFD uses TX GPIO6 and RX GPIO7. The ESP32-S3 TWAI controller supports classical CAN only, so payloads are limited to 8 bytes even though the transceiver hardware is CAN-FD capable.

## Create

```lua
local can = require("can")
local bus = can.new(6, 7, 1000000)
```

Arguments: TX GPIO, RX GPIO, bitrate, optional mode. Bitrates: 25000, 50000, 100000, 125000, 250000, 500000, 800000, 1000000. Modes: `normal` (default), `listen_only`, `no_ack`. Use normal mode on both devices so the receiver supplies the ACK bit.

## Transmit

```lua
bus:transmit(0xF1, {1, 2, 3, 4, 5, 6, 7, 8})
bus:transmit(0x123, string.char(1, 2, 3), {timeout_ms = 200})
```

`transmit(id, data[, options])` accepts a byte table or binary string. Options: `extended`, `rtr`, `single_shot`, `self_receive`, `timeout_ms`. Standard IDs are 0..0x7FF. Set `extended=true` for 29-bit IDs.

## Receive and status

```lua
local frame = bus:receive(1000)
if frame then
    print(string.format("ID=0x%X DLC=%d", frame.id, frame.dlc))
    for i, value in ipairs(frame.data) do print(i, value) end
end
local status = bus:status()
bus:close()
```

`receive(timeout_ms)` returns nil on timeout. Frames contain `id`, `extended`, `rtr`, `dlc`, `data` (byte table), and `payload` (binary string). Rising error counters usually mean missing ACK, mismatched bitrate, reversed CAN_H/CAN_L, missing common ground, or incorrect termination. Only one CAN handle can own the ESP32-S3 TWAI controller.
