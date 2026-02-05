# Agora Conversational AI Agent on ReSpeaker XVF3800

*English | [简体中文](./README.cn.md)*

## Overview

This is an ESP32-S3 based real-time voice conversation client that works with **Agora Conversational AI Agent API v2** on the **ReSpeaker 4-Mic Array with XVF3800** hardware. The project enables natural voice conversations with AI agents powered by LLMs (Large Language Models) through the Agora RTC (Real-Time Communication) platform.

### Key Features

- ✅ **Real-time Voice Conversation** - Low-latency audio streaming via Agora RTC
- ✅ **AI Agent Integration** - Direct integration with Agora Conversational AI Agent API v2
- ✅ **XVF3800 Button Control** - I2C button interface for agent control
- ✅ **AEC (Acoustic Echo Cancellation)** - Built-in echo cancellation for better audio quality
- ✅ **G.711 μ-law Audio** - Efficient audio codec for embedded systems
- ✅ **Configurable AI Backend** - Support for OpenAI, Azure, and other LLM providers
- ✅ **8kHz Sampling Rate** - Optimized for voice conversation

### Hardware Platform

This project is specifically designed for:
- **Board**: Seeed Studio ReSpeaker 4-Mic Array with XVF3800
- **Microcontroller**: Seeed Studio XIAO ESP32-S3
- **Audio Codec**: AIC3104 (via XVF3800)
- **Microphones**: 4-microphone array with XVF3800 DSP

## Table of Contents

- [Hardware Requirements](#hardware-requirements)
- [File Structure](#file-structure)
- [Environment Setup](#environment-setup)
- [Hardware Configuration](#hardware-configuration)
- [Project Configuration](#project-configuration)
- [Compilation and Flashing](#compilation-and-flashing)
- [Usage Guide](#usage-guide)
- [Project Architecture](#project-architecture)
- [Agora Conversational AI API](#agora-conversational-ai-api)
- [About Agora](#about-agora)

---

## Hardware Requirements

### Required Components

1. **ReSpeaker 4-Mic Array with XVF3800**
   - Product: [Seeed Studio ReSpeaker](https://www.seeedstudio.com/ReSpeaker-Mic-Array-v2-0.html)
   - Features: 4 microphones, XVF3800 DSP, AIC3104 codec

2. **Seeed Studio XIAO ESP32-S3**
   - Board: ESP32-S3 with 8MB PSRAM
   - USB-C connection for programming and power

3. **Speaker**
   - Connect to the 3.5mm audio jack on ReSpeaker

### Hardware Connections

The ReSpeaker XVF3800 connects to XIAO ESP32-S3 via:

| Interface | Function | ESP32-S3 Pins |
|-----------|----------|---------------|
| **I2C** | Codec control & buttons | SDA=GPIO5, SCL=GPIO6 |
| **I2S** | Audio data | BCLK=GPIO8, WS=GPIO7, DOUT=GPIO44, DIN=GPIO43 |
| **Power** | 5V USB-C | Via XIAO ESP32-S3 |

---

## File Structure

```
esp32-client-Respeaker-convo/
├── CMakeLists.txt
├── components/
│   └── agora_iot_sdk/                  Agora IoT SDK
│       ├── CMakeLists.txt
│       ├── include/
│       │   └── agora_rtc_api.h
│       └── libs/                       Pre-compiled libraries
│           ├── libagora-cjson.a
│           ├── libahpl.a
│           └── librtsa.a
├── board_configs/                      Hardware configuration files
│   ├── board_pins_config_respeaker.c   ← ReSpeaker configuration
│   └── board_pins_config_korvo_v3.c    ← Korvo-2-V3 configuration
├── main/                               Application code
│   ├── app_config.h                    ← Configuration file (EDIT THIS)
│   ├── llm_main.c                      Main application
│   ├── ai_agent.c                      AI Agent API client
│   ├── ai_agent.h
│   ├── audio_proc.c                    Audio pipeline
│   ├── audio_proc.h
│   ├── rtc_proc.c                      RTC handling
│   ├── rtc_proc.h
│   ├── xvf3800.c                       XVF3800 button driver
│   ├── xvf3800.h
│   ├── common.h
│   └── CMakeLists.txt
├── partitions.csv                      Flash partition table
├── sdkconfig.defaults                  ESP-IDF default config
├── sdkconfig.defaults.esp32s3          ESP32-S3 specific config
├── README.md                           
```

---

## Environment Setup

### Prerequisites

- **Operating System**: Linux (Ubuntu 20.04+) or macOS recommended
- **ESP-IDF**: v5.2.3 commitId c9763f62dd00c887a1a8fafe388db868a7e44069
- **ESP-ADF**: v2.7  commitId 9cf556de500019bb79f3bb84c821fda37668c052
- **Python**: 3.8+

### Step 1: Install ESP-IDF v5.2.3

```bash
# Create esp directory
mkdir -p ~/esp
cd ~/esp

# Clone ESP-IDF
git clone -b v5.2.3 --recursive https://github.com/espressif/esp-idf.git

# Install dependencies
cd esp-idf
./install.sh esp32s3

# Set up environment variables (add to ~/.bashrc or ~/.zshrc)
alias get_idf='. $HOME/esp/esp-idf/export.sh'
```

### Step 2: Install ESP-ADF v2.7

```bash
cd ~/esp

# Clone ESP-ADF
git clone -b v2.7 --recursive https://github.com/espressif/esp-adf.git

# Set environment variable
export ADF_PATH=~/esp/esp-adf
echo 'export ADF_PATH=~/esp/esp-adf' >> ~/.bashrc  # or ~/.zshrc
```

### Step 3: Apply IDF Patch

```bash
cd ~/esp/esp-idf
git apply $ADF_PATH/idf_patches/idf_v5.2_freertos.patch
```

### Step 4: Download Agora IoT SDK

```bash
cd /path/to/esp32-client-Respeaker-convo/components

# Download and extract Agora IoT SDK
wget https://rte-store.s3.amazonaws.com/agora_iot_sdk.tar
tar -xvf agora_iot_sdk.tar
```

After extraction, verify the structure:
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

## Hardware Configuration

### Important: Configure ESP-ADF for ReSpeaker

Before building, you **MUST** configure ESP-ADF to use ReSpeaker pin mappings:

```bash
# Copy ReSpeaker configuration to ESP-ADF
cp board_configs/board_pins_config_respeaker.c \
   ~/esp/esp-adf/components/audio_board/esp32_s3_korvo2_v3/board_pins_config.c
```

**Verify the configuration:**
```bash
grep "TAG =" ~/esp/esp-adf/components/audio_board/esp32_s3_korvo2_v3/board_pins_config.c
```

Expected output:
```
static const char *TAG = "RESPEAKER_XVF3800";
```

> **⚠️ Critical:** If you skip this step, audio will not work! The default ESP-ADF configuration is for Korvo-2-V3, which uses different I2C/I2S pins.

---

## Project Configuration

### Edit `main/app_config.h`

Open `main/app_config.h` and configure the following:

#### 1. WiFi Credentials

```c
#define WIFI_SSID                "Your_WiFi_SSID"
#define WIFI_PASSWORD            "Your_WiFi_Password"
```

#### 2. Agora Account

```c
#define AGORA_APP_ID             "your_agora_app_id"
#define AGORA_API_KEY            "your_agora_api_key"
#define AGORA_API_SECRET         "your_agora_api_secret"
```

**How to get your Agora credentials and enable Conversational AI:**

1. Sign up or log in to [Agora Console](https://console.agora.io/)
2. Create a new project or select an existing project
3. Copy your App ID from the project dashboard
4. Enable **Conversational AI** service in your project settings
5. Generate API credentials (API Key and Secret) for Conversational AI Agent API v2

For detailed instructions, see: [Manage Agora Account - Conversational AI](https://docs.agora.io/en/conversational-ai/get-started/manage-agora-account)

> **Note:** The Conversational AI feature must be enabled in your Agora project before the agent can join RTC channels.

#### 3. RTC Channel

```c
#define CONVO_CHANNEL_NAME       "your_unique_channel"
#define CONVO_RTC_TOKEN          ""  // Optional: Use token for production
#define CONVO_AGENT_RTC_UID      1001
#define CONVO_REMOTE_RTC_UID     1000
```

#### 4. LLM Configuration

**OpenAI Example:**
```c
#define LLM_URL                  "https://api.openai.com/v1/chat/completions"
#define LLM_API_KEY              "sk-your-openai-api-key"
#define LLM_MODEL                "gpt-4o-mini"
#define LLM_SYSTEM_MESSAGE       "You are a helpful AI assistant."
```

**Azure OpenAI Example:**
```c
#define LLM_URL                  "https://your-resource.openai.azure.com/openai/deployments/your-deployment/chat/completions?api-version=2024-02-15-preview"
#define LLM_API_KEY              "your-azure-api-key"
#define LLM_MODEL                "gpt-4"
```

#### 5. TTS Configuration

This project supports **multiple TTS (Text-to-Speech) vendors** with different configuration formats. You can switch between vendors by editing `main/app_config.h`.

**Supported TTS Vendors:**
- **Cartesia** (Default) - High-quality neural TTS with voice cloning
- **Microsoft Azure** - Azure Cognitive Services Speech

Each vendor has its own configuration format. See the [Agora TTS Overview](https://docs.agora.io/en/conversational-ai/models/tts/overview) for all supported vendors and their parameters.

**Cartesia TTS (Default):**
```c
#define USE_TTS_CARTESIA        // Comment this to disable Cartesia
// #define USE_TTS_MICROSOFT    // Uncomment to use Microsoft instead

#define TTS_CARTESIA_VENDOR       "cartesia"
#define TTS_CARTESIA_API_KEY      "sk_car_your_api_key"
#define TTS_CARTESIA_MODEL_ID     "sonic-2"
#define TTS_CARTESIA_VOICE_MODE   "id"
#define TTS_CARTESIA_VOICE_ID     "248be419-c632-4f23-adf1-5324ed7dbf1d"  // Your voice ID
#define TTS_CARTESIA_CONTAINER    "raw"
#define TTS_CARTESIA_SAMPLE_RATE  16000
#define TTS_CARTESIA_LANGUAGE     "en"
```

**Microsoft Azure TTS:**
```c
// #define USE_TTS_CARTESIA     // Comment this to disable Cartesia
#define USE_TTS_MICROSOFT       // Uncomment to use Microsoft

#define TTS_MICROSOFT_VENDOR      "microsoft"
#define TTS_MICROSOFT_API_KEY     "your-azure-speech-key"
#define TTS_MICROSOFT_REGION      "japanwest"
#define TTS_MICROSOFT_VOICE_NAME  "en-US-AndrewMultilingualNeural"
```

> **Note:** Different TTS vendors use different JSON configuration formats in the agent start request. The code automatically builds the correct format based on which vendor is enabled. See `main/ai_agent.c` for implementation details.

#### 6. ASR Configuration

```c
#define ASR_LANGUAGE             "en-US"
```

---

## Compilation and Flashing

### Step 1: Set Up Environment

```bash
# Load ESP-IDF environment
get_idf

# Or if you didn't create the alias:
. $HOME/esp/esp-idf/export.sh
```

### Step 2: Navigate to Project

```bash
cd /path/to/esp32-client-Respeaker-convo
```

### Step 3: Configure WiFi (Optional, if not using app_config.h)

```bash
idf.py menuconfig
```

Navigate to: `Component config` → `Example Configuration` → Configure WiFi SSID/Password

### Step 4: Build

```bash
idf.py set-target esp32s3
idf.py build
```

**Build Output:**
You should see:
```
Project build complete. To flash, run:
  idf.py flash
```

### Step 5: Flash to Device

```bash
# Flash and monitor (Linux/macOS)
idf.py -p /dev/ttyUSB0 flash monitor

# Or on macOS (XIAO ESP32-S3 typically shows as)
idf.py -p /dev/cu.usbmodem* flash monitor

# Windows
idf.py -p COM3 flash monitor
```

**Fix Permission Issues (Linux):**
```bash
sudo usermod -aG dialout $USER
# Logout and login again
```

---

## Usage Guide

### Quick Start

1. **Power On**
   - Connect XIAO ESP32-S3 via USB-C
   - Device will boot and connect to WiFi

2. **Wait for Ready**
   - Serial output shows: `✓ Connected to WiFi`
   - XVF3800 button monitor starts

3. **Start Conversation**
   - Press **SET button** once
   - Agent starts (HTTP 200, agent joins RTC channel)
   - Start speaking!

4. **Stop Conversation**
   - Press **SET button** again (toggle)
   - Or press **MUTE button**


## Project Architecture

### System Architecture Diagram

This diagram shows how the ESP32-S3 board interacts with Agora RTC and AI Agent services:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         ESP32-S3 ReSpeaker Board                            │
│                                                                             │
│  ┌──────────────────────────────────────────────────────────────────────┐  │
│  │                        Hardware Layer                                │  │
│  │                                                                      │  │
│  │  ┌─────────────┐    I2C (SDA/SCL)    ┌──────────────────────┐     │  │
│  │  │  XVF3800    │◄──────────────────► │   XIAO ESP32-S3      │     │  │
│  │  │  (Buttons)  │                     │   (Main MCU)          │     │  │
│  │  └─────────────┘                     │                       │     │  │
│  │                                       │  - WiFi Module        │     │  │
│  │  ┌─────────────┐    I2S (Audio)      │  - 8MB PSRAM         │     │  │
│  │  │  AIC3104    │◄──────────────────► │  - Dual-core 240MHz  │     │  │
│  │  │  (Codec)    │  BCLK/WS/DIN/DOUT   │                       │     │  │
│  │  └─────────────┘                     └──────────────────────┘     │  │
│  │         ▲                                        │                 │  │
│  │         │                                        │                 │  │
│  │  ┌──────┴──────┐                          ┌─────▼────────┐       │  │
│  │  │ 4-Mic Array │                          │   Speaker    │       │  │
│  │  │  (XVF3800)  │                          │   (3.5mm)    │       │  │
│  │  └─────────────┘                          └──────────────┘       │  │
│  └──────────────────────────────────────────────────────────────────┘  │
│                                                                         │
│  ┌──────────────────────────────────────────────────────────────────┐  │
│  │                     Application Layer                            │  │
│  │                                                                  │  │
│  │  ┌─────────────┐  ┌──────────────┐  ┌─────────────────────┐   │  │
│  │  │ llm_main.c  │  │ ai_agent.c   │  │   audio_proc.c      │   │  │
│  │  │             │  │              │  │                     │   │  │
│  │  │ - WiFi Init │  │ - Start/Stop │  │ - I2S Streams       │   │  │
│  │  │ - Button    │  │ - Token Gen  │  │ - AEC Algorithm     │   │  │
│  │  │   Monitor   │  │ - HTTP API   │  │ - Audio Pipeline    │   │  │
│  │  └──────┬──────┘  └──────┬───────┘  └─────────┬───────────┘   │  │
│  │         │                 │                    │               │  │
│  │         └─────────────────┴────────────────────┘               │  │
│  │                           │                                    │  │
│  │                    ┌──────▼────────┐                           │  │
│  │                    │  rtc_proc.c   │                           │  │
│  │                    │               │                           │  │
│  │                    │ - Join/Leave  │                           │  │
│  │                    │   Channel     │                           │  │
│  │                    │ - RTC Events  │                           │  │
│  │                    │ - Audio TX/RX │                           │  │
│  │                    └───────┬───────┘                           │  │
│  └────────────────────────────┼───────────────────────────────────┘  │
│                                │                                      │
│  ┌─────────────────────────────▼──────────────────────────────────┐  │
│  │                    Agora IoT SDK Layer                          │  │
│  │                                                                 │  │
│  │  ┌──────────────────────────────────────────────────────────┐  │  │
│  │  │       Agora RTC API (agora_rtc_api.h)                    │  │  │
│  │  │                                                          │  │  │
│  │  │  - Audio Encode/Decode (G.711 μ-law)                    │  │  │
│  │  │  - RTC Connection Management                            │  │  │
│  │  │  - Audio Frame Push/Pull                                │  │  │
│  │  │  - Event Callbacks                                      │  │  │
│  │  └──────────────────────────────────────────────────────────┘  │  │
│  └─────────────────────────────────────────────────────────────────┘  │
└─────────────────────────────┬───────────────────────────────────────────┘
                              │
                              │ WiFi / Internet
                              │
        ┌─────────────────────┴─────────────────────┐
        │                                           │
        │                                           │
┌───────▼────────────┐                   ┌──────────▼────────────┐
│  Agora SD-RTN      │                   │  Agora AI Agent       │
│                    │                   │                       │
│  ┌──────────────┐  │                   │  ┌─────────────────┐  │
│  │  SD-RTN™     │  │◄──────────────────┼─►│  Agent Manager  │  │
│  │  Network     │  │   Audio Stream    │  │                 │  │
│  │              │  │   (G.711 8kHz)    │  │  - Start/Stop   │  │
│  │ - Global     │  │                   │  │  - Token Auth   │  │
│  │   Routing    │  │                   │  │  - Config       │  │
│  │ - Low        │  │                   │  └─────────────────┘  │
│  │   Latency    │  │                   │                       │
│  │ - High QoS   │  │                   │  ┌─────────────────┐  │
│  └──────────────┘  │                   │  │  AI Pipeline    │  │
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

                Data Flow:
                ─────────►  Outbound: Voice/Audio from ESP32
                ◄─────────  Inbound: AI Response to ESP32
                ◄────────►  Bidirectional: Control & Signaling
```

### Audio Pipeline

```
Microphone (XVF3800)
    ↓ I2S (GPIO43)
I2S Stream Reader
    ↓
Algorithm Stream (AEC)
    ↓
Raw Stream
    ↓
RTC Encoder (G.711)
    ↓
Agora RTC → AI Agent
    ↓
LLM Processing
    ↓
TTS (Azure/other)
    ↓
Agora RTC ← Audio Response
    ↓
Raw Stream
    ↓
I2S Stream Writer
    ↓ I2S (GPIO44)
Speaker (AIC3104)
```

### Component Overview

| Component | Purpose |
|-----------|---------|
| **llm_main.c** | Main application, WiFi, button init |
| **ai_agent.c** | Agora AI Agent API client (start/stop) |
| **audio_proc.c** | Audio pipeline setup (I2S, AEC) |
| **rtc_proc.c** | Agora RTC handling (join/leave channel) |
| **xvf3800.c** | XVF3800 button driver (I2C polling) |
| **board_pins_config.c** | Hardware pin mappings (in ESP-ADF) |

---

## Agora Conversational AI API

This project uses the **Agora Conversational AI Agent API v2** to manage AI agent lifecycle in RTC channels. The implementation is in `main/ai_agent.c`.

### Start Agent Request

The project automatically constructs agent start requests with the following parameters:

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
      "enable_aivad": true  // AI Voice Activity Detection
    },
    "llm": { /* LLM configuration */ },
    "tts": { /* TTS configuration */ },
    "asr": { /* ASR configuration */ }
  }
}
```

### API Reference

For complete parameter documentation and REST API details:

- **Start Agent (Join)**: [Agora Conversational AI - Join API](https://docs.agora.io/en/conversational-ai/rest-api/agent/join)
- **Stop Agent (Leave)**: See the same documentation for leave endpoint
- **Agent State Query**: GET `/agents?state=2` to list running agents

### Implementation Details

The `ai_agent.c` file implements:
- `ai_agent_start()` - Sends POST request to `/join` endpoint with agent configuration
- `ai_agent_stop()` - Sends POST request to `/leave` endpoint to gracefully stop the agent
- `_get_running_agent_id()` - Queries running agents via GET `/agents?state=2`
- `_stop_agent_by_id()` - Stops specific agent by ID to resolve conflicts

---

## About Agora

Agora provides real-time engagement APIs that power billions of minutes of live voice, video, and interactive streaming experiences worldwide. The Agora IoT SDK brings this capability to embedded devices like ESP32, enabling:

- **Low-latency audio** - Sub-second voice transmission
- **Global reach** - Agora SD-RTN™ network in 200+ regions
- **Reliability** - 99.99% uptime with intelligent routing
- **Small footprint** - Optimized for resource-constrained devices

### Resources

- [Agora Console](https://console.agora.io/)
- [Agora Documentation](https://docs.agora.io/)
- [Agora IoT SDK](https://docs.agora.io/en/iot)
- [Conversational AI Agent API](https://docs.agora.io/en/conversational-ai)

---

## License

This project includes:
- Agora IoT SDK (proprietary)
- ESP-IDF (Apache 2.0)
- ESP-ADF (Apache 2.0)

See individual component licenses for details.

