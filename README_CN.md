<div align="center" markdown="1">
  <img src="images/LilyGo_logo.png" alt="LilyGo logo" width="100"/>
</div>
<h1 align = "center">🌟LilyGo-Esp-Claw🌟</h1>

​								**[English](./README.md) | 中文**

<img src="C:\Users\Xinyuan\Desktop\GitHub\LilyGo-Esp-Claw\images\esp-claw.png" alt="esp-claw" style="zoom:25%;" /> 

适配设备：

| T-Connect-Pro | [![alt text](https://camo.githubusercontent.com/aba00c21f632b6ee99f63dba2e64de19d88bea4ac4f4c1f1512ab13d0a72fbc2/68747470733a2f2f696d672e736869656c64732e696f2f62616467652f2d737570706f727465642d677265656e)](https://camo.githubusercontent.com/aba00c21f632b6ee99f63dba2e64de19d88bea4ac4f4c1f1512ab13d0a72fbc2/68747470733a2f2f696d672e736869656c64732e696f2f62616467652f2d737570706f727465642d677265656e) |
| ------------- | ------------------------------------------------------------ |
|               |                                                              |
|               |                                                              |

# 新增

- 2026/08/08 : 

  - 添加 LilyGO T-Connect-Pro 板件配置

  - 添加开发板管理器支持 

  - 添加 ESP32-S3-R8 配置（16MB Flash 和 8MB PSRAM）  

  - 添加 ST7796 LCD 初始化支持  

  - 添加 CST226SE 触摸控制器支持  

  - 添加 SX1262 LoRa 外设配置  

  - 添加通信外设配置，包括：  

    - TWAI CAN
    - RS485 UART

    - RS232 UART

    - W5500 Ethernet

> \[!IMPORTANT]
> If you encounter a problem during use, first check whether the modem's current firmware version is the latest.

# `1` Esp-Claw👋

* **ESP-Claw** 是面向物联网设备的 **Chat Coding（聊天造物）** 式 AI 智能体框架，以对话定义设备行为，在乐鑫芯片上本地完成感知、推理、决策与执行的完整闭环。

# `2` 快速开始

## 使用 Flash download tool 烧录固件

下载 [Flash_download_tool](https://dl.espressif.com/public/flash_download_tool.zip)

![web_flasher](C:\Users\Xinyuan\Desktop\esp-claw\images\esp_downloader.gif)

* 请注意，烧录完成后，您需要按下“RST”键进行重置。

## 使用在线 Web Flasher烧录固件

- [ESP Web Flasher Online](https://espressif.github.io/esptool-js/)

![web_flasher](C:\Users\Xinyuan\Desktop\esp-claw\images\web_flasher.gif)

- 请注意，烧录完成后，您需要按下“RST”键进行重置。

## 使用命令行烧录固件


如果系统提示要安装开发工具，请进行安装。

```bash
python3 -m pip install --upgrade pip
python3 -m pip install esptool
```

为了启动 esptool.py，直接使用以下命令执行即可：

```bash
python3 -m esptool
```

 ESP32-S3 请使用以下命令进行写入操作。

```bash
esptool --chip esp32s3  --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 80m 0x0 firmware.bin
```

# `3` Web 控制台

**`edge_agent` 在 Wi‑Fi 栈就绪后会启动 Web 控制台，提供聊天、查看状态与修改配置功能。**

\[!警告]
	**edge_agent 不是为生产环境准备的。**
	**edge_agent 的 Web 控制台假定你在可信的环境中运行，并且会返回几乎所有信息。**

​	**不要把 Web 配置页端口暴露到公网。**
​	**SoftAP 保留的控制台也有完整权限，请注意配置密码或启用自动关闭 SoftAP 功能。**
​	**服务跑在局域网，默认无 HTTPS。
​	配置页与 NVS 中存有机密令牌，请勿把导出的配置或整机 NVS 转储随意公开。**
​	**配置页可以下载文件系统中几乎所有内容。**

## 如何连接？

1. 连接到已开启的设备的热点 Wi-Fi（设备名称：**esp-claw-xxxxxx**）

2. 在浏览器中打开：[后台](https://192.168.4.1) (192.168.4.1).【联网后可以使用 http://<设备IP>/】

   > \[!提示]]
   >
   > 页面加载后会自动加载当前配置。页面支持多语言切换，以及显示 Wi-Fi 连接状态、设备 IP 等信息。

3. 设置 Wi-Fi 的网络名称（SSID）和密码。

   系统状态页面可以查看 ESP-Claw 的基本状态，包括网络信息、IP 地址、Wi-Fi 模式、SoftAP SSID 和 IP 等。

   > \[!提示]
   >
   > 您的电脑或手机需与设备处于同一局域网内。设置好WiFi连接以后可以使用设备IP地址登录配置页。

4. 配置LLM信息。

## 系统状态

系统状态页面可以查看 ESP-Claw 的基本状态，包括网络信息、IP 地址、Wi-Fi 模式、SoftAP SSID 和 IP 等。

## 在线聊天

在线聊天功能可在不配置额外 IM 通道时与 ESP-Claw 互动。 在线聊天同样支持使用各项功能，以及通过消息触发 Event Router 能力。例如，你可以使用 `/new` 新建 Session 并切换。

在线聊天暂不支持接收图片等附件。

在线聊天不可用？

请检查 Capabilities 管理页面中的 Local IM 是否被禁用。

## 系统设置

### 基础设置

基础设置包括 Wi-Fi 设置和时区设置。

- Wi-Fi 设置可以配置 SSID 和密码。

  重启生效

  - 目前，除 ESP32-C5 外，其他芯片仅支持 2.4 GHz 频段的 Wi-Fi。
  - 密码留空表示当前 Wi-Fi 无密码。

- SoftAP 设置可以调整 SoftAP SSID、密码及启停。

  重启生效

  - ESP-Claw 默认会启用 SoftAP，以便配网或调整配置。
  - SSID 留空表示采用基于 MAC 地址的默认 SSID（`esp-claw-XXXXXX`）。
  - 密码留空表示开放热点。如设置密码，长度至少 8 个字符。
  - 可以配置 SoftAP 在成功连接至 Wi-Fi 后自动关闭。**出于安全考量，建议启用此功能。**

- 高级设置中，时区设置可以调整设备时区。

  重启生效

  - 需为 POSIX TZ string 格式，推荐从[此表格](https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv)中查阅。
  - 例如：北京/香港/新加坡等地的时区为 `CST-8`。
  - 时区设置关系到 Schedule 定时任务的运行、LLM 获取到的时间等。
  - 初次设置时，系统会自动尝试通过浏览器推断当前时区。但此方法推断的时区可能不准确、并不能推断出夏令时等信息。

### LLM 设置 重启生效

LLM 设置可以调整 LLM 的配置，包括供应商、API Key、模型等。 为方便使用，ESP-Claw 内置了一些常用的 LLM 供应商配置，选择这些供应商仅需输入 API Key 和模型名称。

ESP-Claw 同样支持自定义 LLM 供应商。目前，ESP-Claw 支持 OpenAI 兼容 API 和 Anthropic 兼容 API 两种后端类型。 你需要另行提供 Base URL，必要时按照 LLM 供应商的要求配置 Max Tokens 字段名称等。

ESP-Claw 推荐使用 `gpt-5.4`、`qwen3.6-plus`、`deepseek-v4-pro`、`claude-sonnet-4-6` 同等水平或更佳的模型，以充分发挥 ESP-Claw 的潜力。 如需使用其他模型，请注意调整 LLM 高级选项中的「支持视觉输入」等配置，使之与模型支持的功能相匹配。

留意 Base URL 的格式

自定义 LLM 供应商时，Base URL 的路径需要保留到版本号，即：

- 保留 `/chat/completions` （OpenAI 格式）或 `/messages` （Anthropic 格式）**前**的部分；
- 不应以 `/` 结尾。

例如：

```
https://api.openai.com/v1/chat/completionsvvvvvvvv Base URL vvvvvvvhttps://api.openai.com/v1https://api.anthropic.com/v1/messagesvvvvvvvvvv Base URL vvvvvvvvhttps://api.anthropic.com/v1https://api.deepseek.com/chat/completionsvvvvvvv Base URL vvvvvvvhttps://api.deepseek.com
```



### IM 设置 重启生效

IM 设置页面可接入或调整不同的即时通讯平台，包括微信、QQ、飞书、Telegram 等。

#### 微信

如需启用微信，请点击「生成二维码」按钮，使用微信「扫一扫」功能扫描二维码完成配置。配置后，保存并重启即可生效。

高级设置中的微信 Base URL 和 CDN Base URL 一般不需要修改。默认值为

```
https://ilinkai.weixin.qq.com # WeChat Base URLhttps://novac2c.cdn.weixin.qq.com/c2c # WeChat CDN Base URL
```



#### QQ

如需启用 QQ，请在 [QQ开放平台](https://q.qq.com/qqbot/openclaw/login.html) 创建 QQ 机器人，随后将 App ID 和 App Secret 填入对应字段。 配置后，保存并重启即可生效。

#### 飞书

如需启用飞书，请在 [飞书开放平台-创建飞书智能体应用](https://open.feishu.cn/page/launcher?from=backend_oneclick) 创建飞书智能体应用，随后将 App ID 和 App Secret 填入对应字段。 配置后，保存并重启即可生效。

注：暂不支持 Lark 飞书国际版。

#### Telegram

如需启用 Telegram，请与 Telegram APP 内的 [@botfather](https://t.me/botfather) 对话，创建机器人并获取 Bot Token。随后将 Bot Token 填入对应字段。 配置后，保存并重启即可生效。

### 网络和搜索设置

ESP-Claw 可以接入 Brave 或 Tavily 的搜索 API，并在运行中检索在线资源；也支持发起网络请求（GET/POST/…），实时请求在线资源。

**搜索 API Key**：可选，配置后支持通过搜索 API 检索在线资源。

- Brave Search API Key：Brave Search API Key，详见 [Brave API 文档](https://api-dashboard.search.brave.com/documentation/guides/authentication)。
- Tavily API Key：Tavily API Key，详见 [Tavily API 文档](https://docs.tavily.com/documentation/quickstart)。

**HTTP 请求**：需配置 HTTP 白名单，支持通配符*，单独*表示允许全部域名/IP。

## 记忆管理

记忆管理页面用于查看记忆系统中的长期记忆、管理「灵魂」、「身份」与「用户信息」。

- **长期记忆**是系统根据结构化记忆生成的人类可读文件。修改该文件无法直接改变结构化记忆，因此仅提供只读查看功能。
- **灵魂**、**身份**与**用户信息**是可编辑的文件，用于记录设备与用户的个性化信息。

在与 ESP-Claw 对话后，你可以点击「Refresh」按钮或「Refresh All」按钮，刷新记忆文件，查看最新的记忆内容。

[Memory 参考文档](https://esp-claw.com/zh-cn/reference-project/memory)了解长期记忆、「灵魂」、「身份」与「用户信息」的详细信息

## Capabilities 管理

Capabilities 管理页面用于管理 ESP-Claw 的各项 Capability 是否启用。如禁用，对应的 Capability 将不被加载。 默认情况下，所有 Capability 均被启用。你可以通过勾选或取消勾选对应的 Capability 来启用或禁用该 Capability。

为了精简上下文长度，不是所有 Capability 都默认对 LLM 可见。 某些 Capability 提供了 Skill，当 LLM 选择激活对应的 Skill 时，该 Capability 将自动向 LLM 提供其提供的工具。

## Lua 模块管理 重启生效

Lua 模块管理可用于管理**由 IDF 底层模块转译**的 Lua 模块是否启用，其与 Skill 提供的 Lua 模块不同。

[Lua 模块参考文档](https://esp-claw.com/zh-cn/reference-cap/lua-modules)了解 Lua 模块的详细信息

## 文件管理

Web 配置页面提供了简单的文件管理功能，可以在不通过 LLM 的情况下直接查阅和读写文件系统中的文件。 ESP-Claw 运行时的文件系统结构见 [文件系统布局](https://esp-claw.com/zh-cn/reference-project/boot-and-runtime#文件系统)。

默认情况下，文件管理为只读模式，仅当启用「管理员模式」（Dev Mode）后，才能进行写操作。 启用「管理员模式」后，可用于手动添加 Skill、修改自动化规则、修改 Lua 脚本等。

注意：

启用「管理员模式」后，请谨慎操作。误删重要文件、JSON 不合法等操作可能导致系统无法正常运行，甚至无限重启。

注意：

- 在线文件读写有大小限制，具体取决于编译固件时的 `CONFIG_HTTP_MAX_UPLOAD_SIZE` 常量。
- 修改自动化规则后，记得在 Console 执行 `auto reload`（见 [Console 交互](https://esp-claw.com/zh-cn/reference-project/console-usage)）或重启。

# `4 `FAQ

### 启动后没有连上 Wi-Fi

**常见原因：**

- SSID/密码错误：两者均区分大小写。
- 路由器频段不兼容：除 ESP32-C5 外，其他芯片暂仅支持 2.4GHz Wi-Fi。
- Wi-Fi 信号过差。

**排查建议：**

1. 通过 SoftAP 下的 Web 配置页，修改 Wi-Fi SSID 与密码，连接到 2.4G Wi-Fi。

### `ask` 超时或没有回复

**常见原因：**

- 设备访问不了 LLM 云端（DNS、防火墙、地区网络）。
- API Key、backend_type、model 不匹配或过期。
- 模型过慢，超过路由层等待 `claw_core` 的时间（日志里会有线索）。
- Event Router 规则不正确，消息无法正确路由。

**排查建议：**

1. 观察日志中是否有 HTTP / TLS 相关错误。
2. 确认配置中 LLM 配置完整。[➡️配置说明](https://esp-claw.com/zh-cn/reference-project/configuration)
3. 检查 Event Router 规则是否正确，消息是否正确路由。

## Agent 与工具

### Agent 一直无法完成某个任务

**典型症状：**

- Agent 声称自己完成了某个任务，但实际没有完成。后续重复指令，Agent 仍然无法完成。

**常见原因：**

- LLM 可能因为在之前调用了错误的工具，导致后续 LLM 一直参考错误的上下文。
- LLM 的能力欠佳，无法正确完成动作。

**排查建议：**

1. 向 Agent 发送 `/new` 命令，切换至新 Session。
2. 更换能力较强的模型，或缩短历史上下文。

### 模型「看不到」某个工具

**常见原因：**

- 该工具所在 **group** 不在 `claw_cap_set_llm_visible_groups` 白名单内（demo 默认是 `cap_files`、`cap_scheduler`、`cap_lua`、`cap_skill`、`cap_llm_inspect`、`cap_http_request`、`cap_web_search`、`cap_router_mgr`，完整结构化记忆模式还包含 `claw_memory`）。
- 需要 **激活技能** 后，模型才会同时拿到工具文档与对应 group 的可见性。

**排查建议：**

1. `cap list` 确认 descriptor 已注册。
2. 用 `skill --activate <id> --session <当前 session>` 激活后重试对话。

### 模型声称调了工具但没有效果

**常见原因：**

- 模型幻觉，实际未产生 tool call。
- 工具执行失败但回复里未说明。
- 上下文过长导致行为异常。

**排查建议：**

1. 看 `claw_core` 日志里的 tool 名称摘要。
2. 手工 `cap call <name> '<json>'` 复现。
3. 更换能力较强的模型，或缩短历史上下文。

### 自编程等高级功能表现不佳

**常见原因：**

- 所用模型的推理能力不足，无法正确生成 Lua 代码或复杂工具调用。

**排查建议：**

- 自编程、复杂工具组合等功能依赖强推理模型的能力，推荐选用 GPT-5.4 或类似性能的模型以取得最佳体验。
- 如果使用较弱的模型，建议先从简单任务（日常问答、定时提醒等）开始体验。

## 自动化与 Lua

### 规则改了不生效

1. 校验 JSON。
2. `auto reload`。
3. `auto emit_message` / `auto emit_trigger` 做最小用例。

### Lua 执行失败

- 路径是否在受管目录、扩展名是否为 `.lua`。
- 参数是否符合脚本预期。
- 异步任务是否超时（可尝试同步执行）。
