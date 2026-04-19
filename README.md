# Deej32Led

> Wireless (no USB connection needed) volume mixer based on ESP32 with WS2812B visual feedback, Web interface, OTA updates support, and a Captive WiFi portal.

![Version](https://img.shields.io/badge/version-0.75-a78bfa?style=flat-square)
![Platform](https://img.shields.io/badge/platform-ESP32-10b981?style=flat-square)
![Framework](https://img.shields.io/badge/framework-Arduino-ef4444?style=flat-square)
![License](https://img.shields.io/badge/license-MIT-64748b?style=flat-square)

[🇷🇺 **Русская версия**](README_ru.md)

> [!TIP]  
> 🚀 **1-Click Installation:** You can now flash the ESP32 board via USB directly from your browser: **[Go to Web Installer](https://darkassassinua.github.io/Deej32LED/)**

---

## What is it?

Deej32Led is firmware for the ESP32 that turns 5 potentiometers into a hardware volume mixer for your PC. The device connects via Wi-Fi using WebSocket to [deejng](https://github.com/nicholasgasior/deejng) (or compatible clients) and provides real-time visual feedback using a WS2812B LED strip.

Each slider controls the volume of a physical or virtual audio channel. The LEDs smoothly display the current volume level, show peak values, and can be customized with different color themes directly from a built-in web dashboard, without needing to reprogram the device.

---

## Features

### LEDs (Visual Feedback)
- **12 LEDs per channel** (5 channels × 12 = 60 LEDs in total), showing volume from 0 to 100%.
- **Smooth transitions** — built-in smooth interpolation (lerp) when changing volume removes any visual jitter.
- **Peak hold** — the peak LED holds its position for 700 ms, then smoothly falls (1 LED every 40 ms). Easily recognizable by a slightly darker tint.
- **Dim zero-position glow** — the very first LED softly glows even when the slider is at zero, so you can always see where the scale begins.
- **12 color themes** — apply an individual theme to each channel or a global one to all of them at once from the dashboard.
- **VU-meter (Equalizer)** — optional sound level overlay received from the PC (via `vu` WebSocket messages).
- **Boot animation** — a beautiful fading rainbow effect upon powering on.
- **Status indicators** — the strip turns off and uses indicator faders upon connection loss:
  - If there is **no Wi-Fi**, the 2nd fader breathes in Blue.
  - If the **client is not connected (DeejNG/Bridge)**, the 3rd fader breathes in Purple.
  - During **OTA downloads via GitHub**, the 1st fader smoothly fills up with White proportional to the progress.

### Web Control Panel (Dashboard)
- Accessible at **http://deej32led.local** (via mDNS) or via the IP address assigned by your router.
- **Bilingual Interface** — supports Russian and English with on-the-fly switching and language auto-detection from the browser.
- **Individual theme selection** — interactive palettes with 12 themes for each channel.
- **Global themes** — apply a selected color scheme to all channels in 1 click.
- **Brightness slider** — adjust strip brightness (5–255) with real-time saving.
- **Enable / Disable VU-meter**.
- Settings are saved automatically to the internal memory (EEPROM).

### PC Companion Bridge (Python)
- Inside the `tools` folder is **`deej_media_bridge.py`** — a UI utility (also with a bilingual interface) for gesture support. 
- **Hardware slider gestures:** by quickly pulling a slider up-and-down or down-and-up (distinct *gbak* and *gcon* events), you can assign hotkeys or media-keys (Play, Next, Prev, Mute) using the bridge.

### Connectivity and Network
- **WebSocket on port `8765`** — fully compatible with the **deejng / OledDeej** protocol.
- **mDNS** — no need to search for local network IPs, just type `deej32led.local` in your browser.
- **Captive portal (Access Point Mode)** — upon first boot or network loss, the ESP32 creates an open hotspot named `Deej32Led-Setup` (IP: 192.168.4.1). Inside, there is an intuitive web interface for selecting and saving your Wi-Fi credentials.
- **Asynchronous network scanning** — Wi-Fi scanning runs asynchronously in the main `loop()`, preventing any Watchdog timeouts and web server freezes.

### OTA Updates (Over-The-Air)
- **Via PlatformIO** (`esp-ota`) — by calling `pio run -e esp32dev_ota -t upload` from the terminal.
- **Via Web browser (HTTP OTA)** — go to `http://deej32led.local/update`. You can easily Drag & Drop the `.bin` firmware file.

### Power Saving
- CPU frequency is reduced from the standard 240 MHz to **80 MHz** (saves ~50 mA) — this is more than enough for buttery-smooth interface and LED performance.
- Enables **WiFi Modem Sleep** in station mode (saves another 50–80 mA during micro-pauses in data transfer).
- Overall, this makes the device well-suited for portable operation from a lithium battery, like an 18650 cell.

---

## Hardware and Components

| Component | Description |
|------|------|
| Microcontroller | Any ESP32 dev board (e.g., 30-pin or 38-pin DevKit Modules) |
| LED Strip | WS2812B, 60 LEDs. Best if split into 12 LEDs per 5 sliders. |
| Potentiometers | 5 pieces × linear slide potentiometers (10 kOhm) |
| LED Control Pin | Assigned to GPIO **13** |
| Slider Pins | GPIO **36, 39, 34, 35, 32** (These are ADC1, which don't conflict with Wi-Fi) |

### Wiring Guide

```text
ESP32           WS2812B Strip
─────           ─────────────
GPIO 13  ───►  DIN (Data In)
5V       ───►  5V
GND      ───►  GND

ESP32           Potentiometers (×5 pcs)
─────           ───────────────────
3.3V     ──── Pin 1 (connect in parallel to all sliders)
GPIO 36  ──── Pin 2 (Slider #1, center pin / wiper)
GPIO 39  ──── Pin 2 (Slider #2)
GPIO 34  ──── Pin 2 (Slider #3)
GPIO 35  ──── Pin 2 (Slider #4)
GPIO 32  ──── Pin 2 (Slider #5)
GND      ──── Pin 3 (connect in parallel to all sliders)
```

> [!NOTE]
> Pins 36, 39, 34, and 35 on the ESP32 board are designed as **input-only**. They lack integrated pull-up hardware resistors. Because of this, using any external resistors for this particular build is absolutely unneeded.

---

## Flashing and Setup Instructions

### Method 1: 1-Click Flash (Web Installer) — Recommended

The easiest way to flash the firmware onto a new board is via the web installer. No development environment setup needed.

1. Connect the ESP32 to your PC via a USB cable.
2. Open the **[Deej32Led Web Installer](https://darkassassinua.github.io/Deej32LED/)** in your browser (Chrome, Edge, Opera, Brave supported).
3. Click **Connect**, select your device's COM port, and confirm the installation.  
   *(Note: if the process won't start, hold down the `BOOT` button on the ESP32 upon your first connection).*
4. Wait for the installation to finish and proceed to "First boot — Connecting the device to the router".

### Method 2: Manual Build via PlatformIO (For Developers)

**Dependencies and Build Tools:**
- [PlatformIO](https://platformio.org/) (VS Code extension recommended)
- [deejng](https://github.com/nicholasgasior/deejng) or any other desktop client (OledDeej/Websockets).

```bash
# 1. Clone the repository via Git
git clone https://github.com/DarkAssassinUA/Deej32Led.git
cd Deej32Led

# 2. Connect ESP32 via USB cable. Click Build and then Upload.
# Or via PlatformIO terminal:
pio run -e esp32dev -t upload
# Note: some boards require holding the BOOT button until "Connecting..." appears.

# (Future) Wireless flashing (when device is already setup within your home network):
pio run -e esp32dev_ota -t upload
```

### First Boot — Connecting the Device to the Router

1. Power the ESP32 (connecting the LED strip power is optional for now).
2. The LEDs on the edges will blink red — this indicates the device is waiting for configuration. Grab your smartphone or laptop, open your Wi-Fi networks list, and connect to the open hotspot: **`Deej32Led-Setup`**.
3. You should be automatically redirected to the captive portal login page. If not, open your browser and navigate to **http://192.168.4.1**.
4. Click the Scan button, pick your desired home network from the neat list, enter the password, and hit **Connect**.
5. The device will blink, save your settings to internal memory, and reboot into normal operating mode. In seconds, you will be able to connect via your PC client.

### Configuring deejng (on PC)

All you need to do is specify your WebSocket address within the deejng configuration file:
```text
ws://deej32led.local:8765/ws
```
*(If mDNS does not resolve via your network drivers — grab the assigned IP from the serial monitor or your router's client list, for instance, `ws://192.168.X.X:8765/ws`)*

---

## Source Code Architecture

The project has been specifically refactored and divided into several distinct logical modules to simplify maintainability:

```text
src/
├── main.cpp          # Main file: retains only basic setup() and loop() (~130 lines)
├── config.h          # Macros, #defines, and pin assignments
├── globals.h/.cpp    # Externs for global variables and color matrix arrays
├── slider.h          # Heart of mechanics — SliderControl class. Handles ADC reads, float lerp math, and peak hold physics
├── settings.h/.cpp   # EEPROM storage routines and the rainbow boot sequence
├── led_effects.h/.cpp# Status indication effects for idle/disconnected LEDs
├── ws_handler.h/.cpp # WebSocket event handlers
├── ota_manager.h/.cpp# ArduinoOTA initialization and HTTP OTA updates
├── wifi_manager.h/.cpp# Base network interfaces. Safe async WiFi scanning + Captive portal
└── web_server.h/.cpp # Port 80 HTTP server and all static HTML web-interfaces (including the Dashboard, /update, /wifi)
```

---

## Dashboard and Controls API

| HTTP Endpoint | Method | Description / Returns |
|----------|--------|-------------|
| `/` | GET | Renders the primary HTML control dashboard |
| `/data` | GET | `JSON`: returns the exact current state of raw ADC values, volumes, settings, signal strength, and brightness. |
| `/set?theme=N` | GET | Applies selected theme (ID number from 0 to 11) globally |
| `/set?ch=N&theme=M` | GET | Applies a theme (M from 0 to 11) individually to a specified channel (N) |
| `/set?brightness=N`| GET | Sets global LED strip brightness (from 5 to 255) |
| `/set?vu=0\|1` | GET | Enables (1) or disables (0) the VU-meter overlap on the LEDs |
| `/wifi` | GET | Interactive Wi-Fi configuration page |
| `/wifi/status` | GET | `JSON`: returns the currently connected network name, assigned IP, and signal level (RSSI in dBm) |
| `/scan` | GET | Asynchronously pings the controller to scan the Wi-Fi spectrum |
| `/scan/result` | GET | Returns the accumulated scanned networks cache (polling recommended 3 seconds post `/scan`) |
| `/connect` | POST | Commits `ssid` and `pass` credentials to permanent memory (NVS Preferences) |
| `/update` | GET / POST | Graphic OTA update webpage, or POST payload receiver for `.bin` updates |

### Base WebSocket Protocol (Port: 8765)

**Incoming Messages (from PC Client):**
```json
{ "type": "config", "names": ["Master", "Music", "Browser", "Game", "Mic"] }
{ "type": "state",  "vol": [80,60,40,100,0], "mute": [false,false,false,false,true] }
{ "type": "vu",     "levels": [0.8, 0.3, 0.0, 0.5, 0.0] }
```

**Outgoing Telemetry (Periodic transmission to PC):**
```json
{ "type": "update", "vol": [80,60,40,100,0], "mute": [false,false,false,false,true], "bak": [...], "con": [...] }
```

**Serial Debug Output at `115200` baud (Compatible with the legacy `deej` desktop app):**
```text
512|300|1020|0|756
```

---

## Built-in LED Color Palettes

| ID | Theme Name | Visual Gradient Colors |
|---|------|--------|
| **0** | VU Classic | 🟢 Green → 🟡 Yellow → 🔴 Red |
| **1** | Aurora | Teal → Blue → Purple |
| **2** | Ember | Red → Orange → Yellow |
| **3** | Synthwave | Purple → Pink → Blazing White |
| **4** | Ocean | Navy Blue → Deep Azure |
| **5** | Forest | Dark Green → Vivid Lime |
| **6** | Sunset | Bright Orange → Magenta |
| **7** | Cherry | Crimson → Snow Pink |
| **8** | Mint | Gentle Mint → Aquamarine |
| **9** | Ice | Light Blue → Glacier White |
| **10**| Galaxy | Blue → Cosmic Violet → Pink |
| **11**| Toxic | Acid Yellow-Green → Yellow |

---

## Project Library Dependencies

Described within the `platformio.ini` file. Downloaded automatically by PlatformIO upon the first build:

```ini
lib_deps =
    fastled/FastLED @ ^3.10.3
    https://github.com/mathieucarbou/AsyncTCP
    https://github.com/mathieucarbou/ESPAsyncWebServer
    bblanchon/ArduinoJson @ ^7.0.0
```

---

## License

MIT License. The source code is completely free to modify, copy, and redistribute for personal or any other purposes. If you wish, feel free to credit the original concept creators.
