<p align="center">
  <img src="assets/mascot.svg" alt="MynouPAD Mascot" width="110" />
</p>

<h1 align="center">MynouPAD S3 — Master Console v7.2</h1>

<p align="center">
  ESP32-S3 programmable macro pad with native TinyUSB HID + CDC, FFat DuckyScript storage, animated OLED mascot identity, modern web studio, and bidirectional Home Assistant integration.
</p>

---

## Key Features

- **ESP32-S3 Dual-Core @ 240 MHz** with 16 MB Flash (`3 MB APP` + `9.9 MB FFat` partition for scripts and custom key labels).
- **Native TinyUSB HID + CDC**: Emulates a USB keyboard and consumer media controller simultaneously alongside a serial telemetry channel.
- **Multi-Modifier DuckyScript Engine**: Supports chained shortcuts (`CTRL ALT F12`, `CTRL+ALT+SHIFT+M`, `GUI r`), `F1`–`F24`, navigation keys, media controls (`VOLUME_UP`, `VOLUME_DOWN`, `MUTE`), `STRING`, `DELAY`, `HA_TOGGLE`, and `HA_SERVICE`.
- **OLED Mascot Visual Identity (`drawFace`)**: Pixel-art OLED mascot featured in the boot animation, web studio header (animated blinking eye), browser favicon, `/mascot.svg` endpoint, and Home Assistant `entity_picture`.
- **MynouPAD Studio Web UI (`http://mynoupad.local`)**: Dark industrial 3×3 physical key matrix, normal & knob-combo mode toggle, custom key labels stored in `/labels.json`, quick-insert syntax chips, live telemetry bar, remote key trigger, and remote media controls.
- **Bidirectional Home Assistant Integration**:
  - **ESP32 → HA**: Pushes live state and attributes to `sensor.mynoupad_status` (IP, RSSI, volume/mute, bridge status, last macro, uptime, firmware, and mascot avatar).
  - **HA / Web → ESP32**: REST endpoints (`/api/status`, `/api/trigger`, `/api/action`, `/api/labels`) enable remote macro triggering and control.
- **Haptic & Audio Feedback**: Non-blocking 5-pattern vibration motor driver and piezo buzzer tones.

---

## Hardware Pinout

| Component | ESP32-S3 GPIO | Function |
| :--- | :--- | :--- |
| **Rotary Encoder CLK / DT** | `1` / `2` | Volume control & OLED menu navigation |
| **Rotary Encoder SW** | `4` | Mute tap, combo modifier hold, menu long-press |
| **3×3 Key Matrix Rows** | `7`, `6`, `5` | Top (`7-8-9`) to bottom (`1-2-3`) row scan |
| **3×3 Key Matrix Columns** | `15`, `16`, `17` | Column inputs with pull-ups |
| **SSD1306 OLED (I2C)** | `8` (SDA) / `9` (SCL) | `128×64` display (`0x3C`) |
| **Buzzer** | `10` | Startup & macro feedback tones |
| **Haptic Motor** | `18` | Non-blocking tactile vibration patterns |
| **NeoPixel RGB LED** | `48` | Bridge, mute, and macro status indicator |

---

## Setup & Compilation

### 1. Configure Credentials
Copy `secrets.h.example` to `secrets.h` (excluded by `.gitignore`) and fill in your credentials:

```cpp
#ifndef SECRETS_H
#define SECRETS_H

#define SECRET_WIFI_SSID     "YOUR_WIFI_SSID"
#define SECRET_WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#define SECRET_HA_URL        "http://homeassistant.local:8123"
#define SECRET_HA_TOKEN      "Bearer YOUR_HOME_ASSISTANT_TOKEN"

#endif
```

### 2. PlatformIO Configuration
- Board: `esp32-s3-devkitc1-n16r8`
- Build flags: `-D ARDUINO_USB_MODE=0 -D ARDUINO_USB_CDC_ON_BOOT=1`
- Libraries: `Adafruit GFX Library`, `Adafruit SSD1306`, `ESP32Encoder`, `Adafruit NeoPixel`, `ArduinoJson`, `AsyncTCP`, `ESPAsyncWebServer`

---

## Supported DuckyScript Commands

| Command | Example | Description |
| :--- | :--- | :--- |
| `CTRL` / `ALT` / `SHIFT` / `GUI` | `CTRL ALT F12` | Press any combination of modifiers + key (`+` or space separated) |
| Special Keys | `F1`..`F24`, `ENTER`, `TAB`, `ESC`, `SPACE`, `UP`..`RIGHT`, `PRINTSCREEN` | Supported standalone or combined with modifiers |
| `STRING <text>` | `STRING hello` | Types text via USB HID |
| `DELAY <ms>` | `DELAY 150` | Pauses execution in milliseconds |
| Media Controls | `VOLUME_UP`, `VOLUME_DOWN`, `MUTE` | Sends USB HID Consumer Control media keys |
| `HA_TOGGLE <entity_id>` | `HA_TOGGLE light.desk_lamp` | Toggles a Home Assistant entity |
| `HA_SERVICE <domain>.<service> <entity_id>` | `HA_SERVICE scene.turn_on scene.work` | Calls any Home Assistant service |

---

## REST API Endpoints

- `GET /` — MynouPAD Studio Web UI
- `GET /mascot.svg` — Official SVG mascot emblem
- `GET /api/status` — Live device telemetry, FFat usage, macro map, and labels
- `GET /get_macro?btn=<1..9>&mode=<macro|combo>` — Read stored DuckyScript
- `POST /save` — Save or delete a key script (`btn`, `script`, optional `isCombo`)
- `POST /api/labels` — Set custom key display name (`key`, `label`)
- `GET|POST /api/trigger?btn=<1..9>&mode=<macro|combo>` — Execute a stored key macro remotely
- `GET|POST /api/action?cmd=<mute|vol_up|vol_down|beep>` — Remote media or buzzer trigger

---

See [CHANGELOG.md](CHANGELOG.md) for version history.
