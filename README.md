<div align="center" markdown="1">
  <img src="images/LilyGo_logo.png" alt="LilyGo logo" width="100"/>
</div>
<h1 align = "center">🌟LilyGo-Esp-Claw🌟</h1>

​								**English | [中文](./README_CN.md)**

<img src="images\esp-claw.png" alt="esp-claw" style="zoom:25%;" /> 

Compatible device：

| T-Connect-Pro | [![alt text](https://camo.githubusercontent.com/aba00c21f632b6ee99f63dba2e64de19d88bea4ac4f4c1f1512ab13d0a72fbc2/68747470733a2f2f696d672e736869656c64732e696f2f62616467652f2d737570706f727465642d677265656e)](https://camo.githubusercontent.com/aba00c21f632b6ee99f63dba2e64de19d88bea4ac4f4c1f1512ab13d0a72fbc2/68747470733a2f2f696d672e736869656c64732e696f2f62616467652f2d737570706f727465642d677265656e) |
| ------------- | ------------------------------------------------------------ |
|               |                                                              |
|               |                                                              |

# News

- 2026/08/08 : 

  - Add LilyGO T-Connect-Pro board configuration

  - Add board manager support

  - Add ESP32-S3-R8 configuration with 16MB Flash and 8MB PSRAM

  - Add LCD initialization support for ST7796

  - Add CST226SE touch controller support

  - Add SX1262 LoRa peripheral configuration

  - Add communication peripheral configurations including:

    - TWAI CAN

    - RS485 UART

    - RS232 UART

    - W5500 Ethernet

> \[!IMPORTANT]
> If you encounter a problem during use, first check whether the modem's current firmware version is the latest.

# 1` Esp-Claw👋

* ESP-Claw is a Chat Coding AI agent framework for IoT devices. It defines device behavior through conversation and completes the full loop of sensing, reasoning, decision-making, and execution locally on Espressif chips.

# `2` Quick Start

## Use ESP Download Tool

Download [Flash_download_tool](https://dl.espressif.com/public/flash_download_tool.zip)

![web_flasher](images/esp_downloader.gif)

* Note that after writing is completed, you need to press RST to reset.

## Use Web Flasher

- [ESP Web Flasher Online](https://espressif.github.io/esptool-js/)

![web_flasher](images/web_flasher.gif)

- Note that after writing is completed, you need to press RST to reset.

## Use command line


If system asks about install Developer Tools, do it.

```bash
python3 -m pip install --upgrade pip
python3 -m pip install esptool
```

In order to launch esptool.py, exec directly with this:

```bash
python3 -m esptool
```

For ESP32-S3 use the following command to write

```bash
esptool --chip esp32s3  --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 80m 0x0 firmware.bin
```

# `3` Web Console

`edge_agent` starts the Web Console after the Wi-Fi stack is ready, providing chat, status view, and configuration features.

\[!IMPORTANT]
	**`edge_agent` is not intended for production use**
	**The `edge_agent` Web Console assumes a trusted environment and returns almost all information.**

​	**Do not expose the Web configuration page port to the public Internet.**
​	**The retained SoftAP console also has full privileges. Set a password or enable auto-disable SoftAP when possible.**。
​	**The service runs on LAN and does not use HTTPS by default.**
​	**The service runs on LAN and does not use HTTPS by default.**
​	**The configuration page can download almost all content in the filesystem.**

## How to connect?

1. Connect to the hotspot Wi-Fi of the device that has been turned on (name: **esp-claw-xxxxxx**)

2. In the browser open: [link](https://192.168.4.1) (192.168.4.1).

   > \[!TIP]
   >
   > The page loads current settings automatically. The UI supports multi-language switching and displays Wi-Fi status, device IP, and other information.

3. Configure Wi-Fi SSID and password.

   > \[!TIP]
   >
   > Your computer or phone must be on the same LAN as the board.After setting up the WiFi connection, you can log in to the configuration page using the device's IP address.

4. Configure LLM information.


## System Status

The System Status page shows basic ESP-Claw status, including network information, IP address, Wi-Fi mode, SoftAP SSID, and SoftAP IP.

## Web Chat

Web Chat lets you interact with ESP-Claw without configuring extra IM channels. Web Chat also supports regular features and triggering Event Router capabilities through messages. For example, you can use `/new` to create and switch Session.

Web Chat currently does not support receiving image attachments.

Web Chat unavailable?

Check whether Local IM is disabled on the Capabilities management page.

## System Settings

### Basic Settings

Basic settings include Wi-Fi and timezone settings.

- Wi-Fi settings configure SSID and password.

   

  Requires Restart

  - Currently, except ESP32-C5, other chips only support 2.4 GHz Wi-Fi.
  - Empty password means the current Wi-Fi has no password.

- SoftAP settings configure SoftAP SSID, password, and start/stop behavior.

   

  Requires Restart

  - ESP-Claw enables SoftAP by default for provisioning and configuration adjustments.
  - Empty SSID means using the MAC-based default SSID (`esp-claw-XXXXXX`).
  - Empty password means open hotspot. If password is set, it must be at least 8 characters.
  - You can configure SoftAP to auto-disable after successful Wi-Fi connection. **For security, this is recommended.**

- In Advanced settings, timezone settings adjust device timezone.

   

  Requires Restart

  - Must be in POSIX TZ string format. Recommended reference: [this table](https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv).
  - Example: timezone for Beijing/Hong Kong/Singapore is `CST-8`.
  - Timezone affects Schedule task execution and the time provided to LLM.
  - On first setup, the system attempts to infer timezone from browser automatically. This inference may be inaccurate and cannot infer DST details.

### LLM Settings Requires Restart

LLM settings configure provider, API key, model, and other LLM options. For easier onboarding, ESP-Claw includes presets for common LLM providers. For those providers, you only need API key and model name.

ESP-Claw also supports custom LLM providers. Currently, ESP-Claw supports two backend types: OpenAI-compatible API and Anthropic-compatible API. You need to provide Base URL manually, and configure fields like Max Tokens field name if required by the provider.

ESP-Claw recommends models at least comparable to `gpt-5.4`, `qwen3.6-plus`, `deepseek-v4-pro`, and `claude-sonnet-4-6` to unlock its full potential. If you use other models, tune LLM advanced options such as “supports vision input” to match the model capabilities.

Pay attention to Base URL format

For custom LLM providers, the Base URL path must be kept up to the version segment:

- Keep only the part **before** `/chat/completions` (OpenAI format) or `/messages` (Anthropic format);
- Do not end with `/`.

For example:

```
https://api.openai.com/v1/chat/completionsvvvvvvvv Base URL vvvvvvvhttps://api.openai.com/v1https://api.anthropic.com/v1/messagesvvvvvvvvvv Base URL vvvvvvvvhttps://api.anthropic.com/v1https://api.deepseek.com/chat/completionsvvvvvvv Base URL vvvvvvvhttps://api.deepseek.com
```



### IM Settings Requires Restart

The IM settings page lets you connect or adjust different instant messaging platforms, including WeChat, QQ, Feishu, Telegram, and more.

#### WeChat

To enable WeChat, click “Generate QR” and scan it with WeChat “Scan” to complete setup. Save and restart for changes to take effect.

WeChat Base URL and CDN Base URL in Advanced settings usually do not need changes. Default values are:

```
https://ilinkai.weixin.qq.com # WeChat Base URLhttps://novac2c.cdn.weixin.qq.com/c2c # WeChat CDN Base URL
```



#### QQ

To enable QQ, create a QQ bot on [QQ Open Platform](https://q.qq.com/qqbot/openclaw/login.html), then fill App ID and App Secret in the corresponding fields. Save and restart for changes to take effect.

#### Feishu

To enable Feishu, create a Feishu bot app on [Feishu Open Platform - Create Feishu Agent App](https://open.feishu.cn/page/launcher?from=backend_oneclick), then fill App ID and App Secret in the corresponding fields. Save and restart for changes to take effect.

Note: Lark international edition is not supported yet.

#### Telegram

To enable Telegram, chat with [@botfather](https://t.me/botfather) in Telegram app, create a bot, and obtain Bot Token. Then fill the Bot Token field. Save and restart for changes to take effect.

### Network & Search Settings

ESP-Claw can connect to Brave or Tavily search APIs to retrieve online resources during runtime; it can also issue HTTP requests (GET/POST/…) for real-time online access.

**Search API Keys**: optional. Once configured, online resources can be retrieved via search APIs.

- Brave Search API Key: Brave Search API Key, see [Brave API docs](https://api-dashboard.search.brave.com/documentation/guides/authentication).
- Tavily API Key: Tavily API Key, see [Tavily API docs](https://docs.tavily.com/documentation/quickstart).

**HTTP Requests**: configure HTTP allowlist. Wildcard `*` is supported; standalone `*` allows all domains/IPs.

## Memory Management

The Memory Management page is for viewing long-term memory and managing “Soul”, “Identity”, and “User Info” in the memory system.

- **Long-term Memory** is a human-readable file generated from structured memory. Editing this file does not directly modify structured memory, so it is read-only for inspection.
- **Soul**, **Identity**, and **User Info** are editable files used to store personalized information for the device and user.

After chatting with ESP-Claw, click “Refresh” or “Refresh All” to refresh memory files and view the latest memory content.

[Memory Reference](https://esp-claw.com/en/reference-project/memory)Learn details about long-term memory, Soul, Identity, and User Info

## Capabilities Management

The Capabilities Management page controls whether each ESP-Claw capability is enabled. If disabled, the capability is not loaded. By default, all capabilities are enabled. You can enable or disable each capability by checking or unchecking it.

To reduce context length, not all capabilities are LLM-visible by default. Some capabilities provide Skills. When LLM chooses to activate the corresponding Skill, that capability automatically provides its tools to the LLM.

## Lua Modules Management Requires Restart

Lua Modules Management controls whether Lua modules **translated from IDF low-level modules** are enabled. These are different from Lua modules provided by Skills.

[Lua Modules Reference](https://esp-claw.com/en/reference-cap/lua-modules)Learn details about Lua modules

## File Management

The Web configuration page provides simple file management, so you can browse and read/write filesystem files directly without going through LLM. For ESP-Claw runtime filesystem structure, see [Filesystem layout](https://esp-claw.com/en/reference-project/boot-and-runtime#文件系统).

By default, File Management is read-only. Write actions are enabled only after turning on “Admin Mode” (Dev Mode). After enabling “Admin Mode”, you can manually add Skills, edit automation rules, modify Lua scripts, and more.

Warning

After enabling “Admin Mode”, operate carefully. Deleting important files or invalid JSON may cause system failures, even reboot loops.

Note

- Online file read/write has size limits, depending on `CONFIG_HTTP_MAX_UPLOAD_SIZE` when firmware is built.
- After changing automation rules, remember to run `auto reload` in Console (see [Console usage](https://esp-claw.com/en/reference-project/console-usage)) or restart.

# `5 `FAQ

### Wi‑Fi does not connect after boot

**Typical causes:**

- Wrong SSID/password (case-sensitive).
- Router band mismatch: except ESP32-C5, only 2.4 GHz Wi‑Fi is supported today.
- Weak signal.

**What to try:**

1. Open the Web config page under SoftAP, update Wi-Fi SSID and password, and connect to 2.4 GHz Wi-Fi.

### `ask` times out or never replies

**Typical causes:**

- Device cannot reach the LLM cloud (DNS, firewall, region).
- API key / backend_type / model mismatch or expired key.
- Model slower than router wait for `claw_core` (check logs).
- Event Router rules mis-route messages.

**What to try:**

1. Look for HTTP/TLS errors in logs.
2. Confirm LLM settings are complete. [➡️ Configuration](https://esp-claw.com/en/reference-project/configuration)
3. Validate Event Router rules and routing.

### Messages sent via IM get no response

**Typical causes:**

- ESP-Claw never receives IM traffic or cannot call the IM API to send.
- LLM runtime failure.
- Event Router rules mis-route messages.

**What to try:**

1. Check HTTP status and error text in logs.
2. Confirm the IM message was ingested—serial logs should show it. [➡️ ESP-Claw cannot send/receive IM](https://esp-claw.com/en/tutorial/faq/#esp-claw-cannot-send-or-receive-im)
3. Run `ask "hello"` on serial; if that works, the LLM path is OK. [➡️ `ask` times out](https://esp-claw.com/en/tutorial/faq/#ask-times-out-or-never-replies)
4. Re-check Event Router rules and routing.

### ESP-Claw cannot send or receive IM

**Typical causes:**

- Wrong IM credentials or expired secret/token.
- Device cannot reach IM platform APIs (Telegram often needs solid connectivity).
- Feishu bot permissions misconfigured.

**What to try:**

1. Inspect HTTP status/errors in logs.
2. Re-enter IM settings and reboot.
3. Verify Feishu bot permissions.

## Agent and tools

### The Agent cannot complete a task

**Typical symptoms:**

- The Agent claims it completed a task but did not. Subsequent instructions, the Agent still cannot complete.

**Common causes:**

- The LLM may have called the wrong tool in the previous turn, causing the LLM to continue to reference the wrong context.
- The LLM’s ability is not strong enough to correctly complete the action.

**What to try:**

1. Send `/new` command to switch to a new Session.
2. Use a stronger model or shorten history.

### The model “does not see” a tool

**Typical causes:**

- The tool’s **group** is outside `claw_cap_set_llm_visible_groups` (demo defaults to `cap_files`, `cap_scheduler`, `cap_lua`, `cap_skill`, `cap_llm_inspect`, `cap_http_request`, `cap_web_search`, `cap_router_mgr`; full structured-memory mode also includes `claw_memory`).
- A **Skill must be activated** so the model gets both the tool docs and that group’s visibility.

**What to try:**

1. `cap list` to confirm descriptors registered.
2. `skill --activate <id> --session <current session>` then retry chat.

### The model claims it called a tool but nothing happens

**Typical causes:**

- Hallucinated tool call—no real tool invocation.
- Tool failed but the reply hid the error.
- Overlong context causing odd behavior.

**What to try:**

1. Read `claw_core` logs for tool name summaries.
2. Manually `cap call <name> '<json>'`.
3. Use a stronger model or shorten history.

### Self-programming and other advanced features underperform

**Typical causes:**

- The model’s reasoning is too weak to reliably generate Lua code or complex tool calls.

**What to try:**

- Self-programming and complex tool orchestration depend on strong reasoning models; we recommend GPT-5.4 or similarly capable models for the best experience.
- With weaker models, start with simpler tasks (daily Q&A, reminders, and so on).

## Automation and Lua

### Rule changes do not apply

1. Validate JSON.
2. `auto reload`.
3. `auto emit_message` / `auto emit_trigger` for a minimal repro.

### Lua execution fails

- Path under the managed root? Extension `.lua`?
- Args match script expectations?
- Async task timing out (try synchronous run).







