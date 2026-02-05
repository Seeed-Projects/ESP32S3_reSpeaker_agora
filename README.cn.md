# ReSpeaker XVF3800 上的 Agora 对话式AI代理

*[English](./README.md) | 简体中文*

## 概述

这是一个运行在 **ReSpeaker 4麦克风阵列（XVF3800）** 硬件上的基于 ESP32-S3 的实时语音对话客户端，通过 **Agora 对话式AI代理 API v2** 实现与大语言模型（LLM）驱动的AI代理进行自然语音对话。该项目通过 Agora RTC（实时通信）平台实现自然语音对话。

### 主要特性

- ✅ **实时语音对话** - 通过 Agora RTC 实现低延迟音频流传输
- ✅ **AI代理集成** - 直接集成 Agora 对话式AI代理 API v2
- ✅ **XVF3800 按钮控制** - I2C 按钮接口控制代理
- ✅ **回声消除（AEC）** - 内置回声消除提升音频质量
- ✅ **G.711 μ-law 音频** - 嵌入式系统高效音频编解码
- ✅ **可配置AI后端** - 支持 OpenAI、Azure 等多种 LLM 提供商
- ✅ **8kHz 采样率** - 针对语音对话优化

### 硬件平台

本项目专为以下硬件设计：
- **开发板**: Seeed Studio ReSpeaker 4麦克风阵列（XVF3800）
- **微控制器**: Seeed Studio XIAO ESP32-S3
- **音频编解码器**: AIC3104（通过 XVF3800）
- **麦克风**: 4麦克风阵列配 XVF3800 DSP

## 目录

- [硬件要求](#硬件要求)
- [文件结构](#文件结构)
- [环境配置](#环境配置)
- [硬件配置](#硬件配置)
- [项目配置](#项目配置)
- [编译和烧录](#编译和烧录)
- [使用指南](#使用指南)
- [项目架构](#项目架构)
- [Agora Conversational AI API](#agora-conversational-ai-api)
- [关于 Agora](#关于-agora)

---

## 硬件要求

### 所需组件

1. **ReSpeaker 4麦克风阵列（XVF3800）**
   - 产品: [Seeed Studio ReSpeaker](https://www.seeedstudio.com/ReSpeaker-Mic-Array-v2-0.html)
   - 特性: 4个麦克风、XVF3800 DSP、AIC3104 编解码器

2. **Seeed Studio XIAO ESP32-S3**
   - 开发板: ESP32-S3 配 8MB PSRAM
   - USB-C 连接用于编程和供电

3. **扬声器**
   - 连接到 ReSpeaker 的 3.5mm 音频插孔

### 硬件连接

ReSpeaker XVF3800 通过以下接口连接到 XIAO ESP32-S3：

| 接口 | 功能 | ESP32-S3 引脚 |
|-----------|----------|---------------|
| **I2C** | 编解码器控制和按钮 | SDA=GPIO5, SCL=GPIO6 |
| **I2S** | 音频数据 | BCLK=GPIO8, WS=GPIO7, DOUT=GPIO44, DIN=GPIO43 |
| **电源** | 5V USB-C | 通过 XIAO ESP32-S3 |

---

## 文件结构

```
esp32-client-Respeaker-convo/
├── CMakeLists.txt
├── components/
│   └── agora_iot_sdk/                  Agora IoT SDK
│       ├── CMakeLists.txt
│       ├── include/
│       │   └── agora_rtc_api.h
│       └── libs/                       预编译库
│           ├── libagora-cjson.a
│           ├── libahpl.a
│           └── librtsa.a
├── board_configs/                      硬件配置文件
│   ├── board_pins_config_respeaker.c   ← ReSpeaker 配置
│   └── board_pins_config_korvo_v3.c    ← Korvo-2-V3 配置
├── main/                               应用代码
│   ├── app_config.h                    ← 配置文件（需编辑）
│   ├── llm_main.c                      主应用程序
│   ├── ai_agent.c                      AI Agent API 客户端
│   ├── ai_agent.h
│   ├── audio_proc.c                    音频处理管道
│   ├── audio_proc.h
│   ├── rtc_proc.c                      RTC 处理
│   ├── rtc_proc.h
│   ├── xvf3800.c                       XVF3800 按钮驱动
│   ├── xvf3800.h
│   ├── common.h
│   └── CMakeLists.txt
├── partitions.csv                      Flash 分区表
├── sdkconfig.defaults                  ESP-IDF 默认配置
├── sdkconfig.defaults.esp32s3          ESP32-S3 专用配置
├── README.md
```

---

## 环境配置

### 前置要求

- **操作系统**: 推荐 Linux (Ubuntu 20.04+) 或 macOS
- **ESP-IDF**: v5.2.3 commitId c9763f62dd00c887a1a8fafe388db868a7e44069
- **ESP-ADF**: v2.7  commitId 9cf556de500019bb79f3bb84c821fda37668c052
- **Python**: 3.8+

### 步骤 1: 安装 ESP-IDF v5.2.3

```bash
# 创建 esp 目录
mkdir -p ~/esp
cd ~/esp

# 克隆 ESP-IDF
git clone -b v5.2.3 --recursive https://github.com/espressif/esp-idf.git

# 安装依赖
cd esp-idf
./install.sh esp32s3

# 设置环境变量（添加到 ~/.bashrc 或 ~/.zshrc）
alias get_idf='. $HOME/esp/esp-idf/export.sh'
```

### 步骤 2: 安装 ESP-ADF v2.7

```bash
cd ~/esp

# 克隆 ESP-ADF
git clone -b v2.7 --recursive https://github.com/espressif/esp-adf.git

# 设置环境变量
export ADF_PATH=~/esp/esp-adf
echo 'export ADF_PATH=~/esp/esp-adf' >> ~/.bashrc  # 或 ~/.zshrc
```

### 步骤 3: 应用 IDF 补丁

```bash
cd ~/esp/esp-idf
git apply $ADF_PATH/idf_patches/idf_v5.2_freertos.patch
```

### 步骤 4: 下载 Agora IoT SDK

```bash
cd /path/to/esp32-client-Respeaker-convo/components

# 下载并解压 Agora IoT SDK
wget https://rte-store.s3.amazonaws.com/agora_iot_sdk.tar
tar -xvf agora_iot_sdk.tar
```

解压后，验证目录结构：
```
components/agora_iot_sdk/
├── CMakeLists.txt
├── include/
│   └── agora_rtc_api.h
└── libs/
    ├── libagora-cjson.a
    ├── libahpl.a
    └── librtsa.a
```

---

## 硬件配置

### 重要：为 ReSpeaker 配置 ESP-ADF

在编译之前，**必须**配置 ESP-ADF 使用 ReSpeaker 引脚映射：

```bash
# 复制 ReSpeaker 配置到 ESP-ADF
cp board_configs/board_pins_config_respeaker.c \
   ~/esp/esp-adf/components/audio_board/esp32_s3_korvo2_v3/board_pins_config.c
```

**验证配置：**
```bash
grep "TAG =" ~/esp/esp-adf/components/audio_board/esp32_s3_korvo2_v3/board_pins_config.c
```

预期输出：
```
static const char *TAG = "RESPEAKER_XVF3800";
```

> **⚠️ 关键：** 如果跳过此步骤，音频将无法工作！ESP-ADF 默认配置是为 Korvo-2-V3 准备的，使用不同的 I2C/I2S 引脚。

---

## 项目配置

### 编辑 `main/app_config.h`

打开 `main/app_config.h` 并配置以下内容：

#### 1. WiFi 凭据

```c
#define WIFI_SSID                "Your_WiFi_SSID"
#define WIFI_PASSWORD            "Your_WiFi_Password"
```

#### 2. Agora 账户

```c
#define AGORA_APP_ID             "your_agora_app_id"
#define AGORA_API_KEY            "your_agora_api_key"
#define AGORA_API_SECRET         "your_agora_api_secret"
```

**如何获取 Agora 凭据并启用对话式 AI 功能：**

1. 注册或登录 [Agora Console](https://console.agora.io/)
2. 创建新项目或选择现有项目
3. 从项目仪表板复制 App ID
4. 在项目设置中启用 **Conversational AI（对话式 AI）** 服务
5. 为 Conversational AI Agent API v2 生成 API 凭据（API Key 和 Secret）

详细说明请参考：[管理 Agora 账户 - 对话式 AI](https://docs.agora.io/en/conversational-ai/get-started/manage-agora-account)

> **注意：** 在 AI 代理可以加入 RTC 频道之前，必须在 Agora 项目中启用对话式 AI 功能。

#### 3. RTC 频道

```c
#define CONVO_CHANNEL_NAME       "your_unique_channel"
#define CONVO_RTC_TOKEN          ""  // 可选：生产环境使用 token
#define CONVO_AGENT_RTC_UID      1001
#define CONVO_REMOTE_RTC_UID     1000
```

#### 4. LLM 配置

**OpenAI 示例：**
```c
#define LLM_URL                  "https://api.openai.com/v1/chat/completions"
#define LLM_API_KEY              "sk-your-openai-api-key"
#define LLM_MODEL                "gpt-4o-mini"
#define LLM_SYSTEM_MESSAGE       "You are a helpful AI assistant."
```

**Azure OpenAI 示例：**
```c
#define LLM_URL                  "https://your-resource.openai.azure.com/openai/deployments/your-deployment/chat/completions?api-version=2024-02-15-preview"
#define LLM_API_KEY              "your-azure-api-key"
#define LLM_MODEL                "gpt-4"
```

#### 5. TTS 配置

本项目支持**多种 TTS（文本转语音）供应商**，每种供应商有不同的配置格式。您可以通过编辑 `main/app_config.h` 来切换供应商。

**支持的 TTS 供应商：**
- **Cartesia**（默认）- 高质量神经 TTS，支持语音克隆
- **Microsoft Azure** - Azure 认知服务语音

每种供应商都有自己的配置格式。有关所有支持的供应商及其参数，请参阅 [Agora TTS 概览](https://docs.agora.io/en/conversational-ai/models/tts/overview)。

**Cartesia TTS（默认）：**
```c
#define USE_TTS_CARTESIA        // 注释此行以禁用 Cartesia
// #define USE_TTS_MICROSOFT    // 取消注释以使用 Microsoft

#define TTS_CARTESIA_VENDOR       "cartesia"
#define TTS_CARTESIA_API_KEY      "sk_car_your_api_key"
#define TTS_CARTESIA_MODEL_ID     "sonic-2"
#define TTS_CARTESIA_VOICE_MODE   "id"
#define TTS_CARTESIA_VOICE_ID     "248be419-c632-4f23-adf1-5324ed7dbf1d"  // 您的语音 ID
#define TTS_CARTESIA_CONTAINER    "raw"
#define TTS_CARTESIA_SAMPLE_RATE  16000
#define TTS_CARTESIA_LANGUAGE     "en"
```

**Microsoft Azure TTS：**
```c
// #define USE_TTS_CARTESIA     // 注释此行以禁用 Cartesia
#define USE_TTS_MICROSOFT       // 取消注释以使用 Microsoft

#define TTS_MICROSOFT_VENDOR      "microsoft"
#define TTS_MICROSOFT_API_KEY     "your-azure-speech-key"
#define TTS_MICROSOFT_REGION      "japanwest"
#define TTS_MICROSOFT_VOICE_NAME  "en-US-AndrewMultilingualNeural"
```

> **注意：** 不同的 TTS 供应商在代理启动请求中使用不同的 JSON 配置格式。代码会根据启用的供应商自动构建正确的格式。实现细节请参见 `main/ai_agent.c`。

#### 6. ASR 配置

```c
#define ASR_LANGUAGE             "en-US"
```

---

## 编译和烧录

### 步骤 1: 设置环境

```bash
# 加载 ESP-IDF 环境
get_idf

# 或者如果没有创建别名：
. $HOME/esp/esp-idf/export.sh
```

### 步骤 2: 导航到项目目录

```bash
cd /path/to/esp32-client-Respeaker-convo
```

### 步骤 3: 配置 WiFi（可选，如果不使用 app_config.h）

```bash
idf.py menuconfig
```

导航到：`Component config` → `Example Configuration` → 配置 WiFi SSID/密码

### 步骤 4: 编译

```bash
idf.py set-target esp32s3
idf.py build
```

**编译输出：**
您应该看到：
```
Project build complete. To flash, run:
  idf.py flash
```

### 步骤 5: 烧录到设备

```bash
# 烧录并监控（Linux/macOS）
idf.py -p /dev/ttyUSB0 flash monitor

# 或在 macOS 上（XIAO ESP32-S3 通常显示为）
idf.py -p /dev/cu.usbmodem* flash monitor

# Windows
idf.py -p <port> flash monitor eg:idf.py -p COM3 flash monitor 
```

**修复权限问题（Linux）：**
```bash
sudo usermod -aG dialout $USER
# 登出并重新登录
```

---

## 使用指南

### 快速开始

1. **上电**
   - 通过 USB-C 连接 XIAO ESP32-S3
   - 设备将启动并连接到 WiFi

2. **等待就绪**
   - 串口输出显示：`✓ Connected to WiFi`
   - XVF3800 按钮监控启动

3. **开始对话**
   - 按一次 **SET 按钮**
   - Agent 启动（HTTP 200，agent 加入 RTC 频道）
   - 开始说话！

4. **停止对话**
   - 再次按 **SET 按钮**（切换）


## 项目架构

### 系统架构图

此图展示了 ESP32-S3 开发板如何与 Agora RTC 和 AI Agent 服务进行交互：

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         ESP32-S3 ReSpeaker 开发板                           │
│                                                                             │
│  ┌──────────────────────────────────────────────────────────────────────┐  │
│  │                        硬件层                                        │  │
│  │                                                                      │  │
│  │  ┌─────────────┐    I2C (SDA/SCL)    ┌──────────────────────┐     │  │
│  │  │  XVF3800    │◄──────────────────► │   XIAO ESP32-S3      │     │  │
│  │  │  (按钮)     │                     │   (主控MCU)          │     │  │
│  │  └─────────────┘                     │                       │     │  │
│  │                                       │  - WiFi 模块         │     │  │
│  │  ┌─────────────┐    I2S (音频)       │  - 8MB PSRAM         │     │  │
│  │  │  AIC3104    │◄──────────────────► │  - 双核 240MHz       │     │  │
│  │  │  (编解码器) │  BCLK/WS/DIN/DOUT   │                       │     │  │
│  │  └─────────────┘                     └──────────────────────┘     │  │
│  │         ▲                                        │                 │  │
│  │         │                                        │                 │  │
│  │  ┌──────┴──────┐                          ┌─────▼────────┐       │  │
│  │  │ 4麦克风阵列 │                          │   扬声器     │       │  │
│  │  │  (XVF3800)  │                          │   (3.5mm)    │       │  │
│  │  └─────────────┘                          └──────────────┘       │  │
│  └──────────────────────────────────────────────────────────────────┘  │
│                                                                         │
│  ┌──────────────────────────────────────────────────────────────────┐  │
│  │                     应用层                                       │  │
│  │                                                                  │  │
│  │  ┌─────────────┐  ┌──────────────┐  ┌─────────────────────┐   │  │
│  │  │ llm_main.c  │  │ ai_agent.c   │  │   audio_proc.c      │   │  │
│  │  │             │  │              │  │                     │   │  │
│  │  │ - WiFi初始化│  │ - 启动/停止  │  │ - I2S 流            │   │  │
│  │  │ - 按钮      │  │ - Token生成  │  │ - AEC 算法          │   │  │
│  │  │   监控      │  │ - HTTP API   │  │ - 音频管道          │   │  │
│  │  └──────┬──────┘  └──────┬───────┘  └─────────┬───────────┘   │  │
│  │         │                 │                    │               │  │
│  │         └─────────────────┴────────────────────┘               │  │
│  │                           │                                    │  │
│  │                    ┌──────▼────────┐                           │  │
│  │                    │  rtc_proc.c   │                           │  │
│  │                    │               │                           │  │
│  │                    │ - 加入/离开   │                           │  │
│  │                    │   频道        │                           │  │
│  │                    │ - RTC 事件    │                           │  │
│  │                    │ - 音频 TX/RX  │                           │  │
│  │                    └───────┬───────┘                           │  │
│  └────────────────────────────┼───────────────────────────────────┘  │
│                                │                                      │
│  ┌─────────────────────────────▼──────────────────────────────────┐  │
│  │                    Agora IoT SDK 层                             │  │
│  │                                                                 │  │
│  │  ┌──────────────────────────────────────────────────────────┐  │  │
│  │  │       Agora RTC API (agora_rtc_api.h)                    │  │  │
│  │  │                                                          │  │  │
│  │  │  - 音频编码/解码 (G.711 μ-law)                          │  │  │
│  │  │  - RTC 连接管理                                         │  │  │
│  │  │  - 音频帧推送/拉取                                      │  │  │
│  │  │  - 事件回调                                             │  │  │
│  │  └──────────────────────────────────────────────────────────┘  │  │
│  └─────────────────────────────────────────────────────────────────┘  │
└─────────────────────────────┬───────────────────────────────────────────┘
                              │
                              │ WiFi / 互联网
                              │
        ┌─────────────────────┴─────────────────────┐
        │                                           │
        │                                           │
┌───────▼────────────┐                   ┌──────────▼────────────┐
│  Agora SD-RTN      │                   │  Agora AI Agent       │
│                    │                   │                       │
│  ┌──────────────┐  │                   │  ┌─────────────────┐  │
│  │  SD-RTN™     │  │◄──────────────────┼─►│  Agent 管理器   │  │
│  │  网络        │  │   音频流          │  │                 │  │
│  │              │  │   (G.711 8kHz)    │  │  - 启动/停止    │  │
│  │ - 全球       │  │                   │  │  - Token 鉴权   │  │
│  │   路由       │  │                   │  │  - 配置         │  │
│  │ - 低         │  │                   │  └─────────────────┘  │
│  │   延迟       │  │                   │                       │
│  │ - 高 QoS     │  │                   │  ┌─────────────────┐  │
│  └──────────────┘  │                   │  │  AI 处理管道    │  │
│                    │                   │  │                 │  │
│                    │                   │  │  ASR ──► LLM   │  │
│                    │                   │  │          │      │  │
│                    │                   │  │          ▼      │  │
│                    │                   │  │         TTS     │  │
│                    │                   │  │                 │  │
│                    │                   │  │  - OpenAI      │  │
│                    │                   │  │  - Azure       │  │
│                    │                   │  │  - Gemini      │  │
│                    │                   │  └─────────────────┘  │
└────────────────────┘                   └───────────────────────┘

                数据流向:
                ─────────►  出站: 从 ESP32 发送语音/音频
                ◄─────────  入站: AI 响应到 ESP32
                ◄────────►  双向: 控制与信令
```

### 音频处理管道

```
麦克风（XVF3800）
    ↓ I2S (GPIO43)
I2S 流读取器
    ↓
算法流（AEC）
    ↓
原始流
    ↓
RTC 编码器（G.711）
    ↓
Agora RTC → AI Agent
    ↓
LLM 处理
    ↓
TTS（Azure/其他）
    ↓
Agora RTC ← 音频响应
    ↓
原始流
    ↓
I2S 流写入器
    ↓ I2S (GPIO44)
扬声器（AIC3104）
```

### 组件概览

| 组件 | 用途 |
|-----------|---------|
| **llm_main.c** | 主应用程序、WiFi、按钮初始化 |
| **ai_agent.c** | Agora AI Agent API 客户端（启动/停止）|
| **audio_proc.c** | 音频管道设置（I2S、AEC）|
| **rtc_proc.c** | Agora RTC 处理（加入/离开频道）|
| **xvf3800.c** | XVF3800 按钮驱动（I2C 轮询）|
| **board_pins_config.c** | 硬件引脚映射（在 ESP-ADF 中）|

---

## Agora Conversational AI API

本项目使用 **Agora Conversational AI Agent API v2** 来管理 RTC 频道中的 AI 代理生命周期。实现代码位于 `main/ai_agent.c`。

### 启动代理请求

项目会自动构建包含以下参数的代理启动请求：

```json
{
  "name": "esp32_agent",
  "properties": {
    "channel": "your_channel_name",
    "token": "optional_rtc_token",
    "agent_rtc_uid": 1001,
    "remote_rtc_uids": [1002],
    "parameters": {
      "output_audio_codec": "PCMU"  // G.711 μ-law
    },
    "idle_timeout": 120,
    "advanced_features": {
      "enable_aivad": true  // AI 语音活动检测
    },
    "llm": { /* LLM 配置 */ },
    "tts": { /* TTS 配置 */ },
    "asr": { /* ASR 配置 */ }
  }
}
```

### API 参考

有关完整的参数文档和 REST API 详情：

- **启动代理（Join）**：[Agora Conversational AI - Join API](https://docs.agora.io/en/conversational-ai/rest-api/agent/join)
- **停止代理（Leave）**：参见同一文档中的 leave 端点
- **代理状态查询**：GET `/agents?state=2` 列出正在运行的代理

### 实现细节

`ai_agent.c` 文件实现了：
- `ai_agent_start()` - 向 `/join` 端点发送 POST 请求，包含代理配置
- `ai_agent_stop()` - 向 `/leave` 端点发送 POST 请求，优雅地停止代理
- `_get_running_agent_id()` - 通过 GET `/agents?state=2` 查询正在运行的代理
- `_stop_agent_by_id()` - 按 ID 停止特定代理以解决冲突

---

## 关于 Agora

Agora 提供实时互动 API，为全球数十亿分钟的实时语音、视频和互动流媒体体验提供支持。Agora IoT SDK 将此功能引入 ESP32 等嵌入式设备，实现：

- **低延迟音频** - 亚秒级语音传输
- **全球覆盖** - Agora SD-RTN™ 网络覆盖 200+ 地区
- **可靠性** - 99.99% 正常运行时间和智能路由
- **小体积** - 针对资源受限设备优化

### 资源

- [Agora Console](https://console.agora.io/)
- [Agora Documentation](https://docs.agora.io/)
- [Agora IoT SDK](https://docs.agora.io/en/iot)
- [Conversational AI Agent API](https://docs.agora.io/en/conversational-ai)

---

## 许可证

本项目包含：
- Agora IoT SDK（专有）
- ESP-IDF（Apache 2.0）
- ESP-ADF（Apache 2.0）

详情请参阅各组件的许可证。

---
