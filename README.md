# ⌨️ MynouPAD S3 — Master Console v6.0

**MynouPAD** is an intelligent, high-performance macro pad and automation console developed for the **ESP32-S3** platform. Combining native USB keyboard and media emulation, a dedicated internal flash partition for automation scripts, a web administration dashboard, and direct **Home Assistant** integration, it elevates productivity and infrastructure control to the next level.

---

## 🚀 Key Features

- **⚡ Powerful Hardware**: Based on the ESP32-S3 Dual-Core @ 240MHz with 16MB of Flash memory.
- **📁 Dedicated Storage (FFat)**: Dedicated 9.9MB partition to store macro script files independently.
- **🖥️ OLED Graphical Interface**: SSD1306 OLED display (128x64) via I2C showing volume status, mute status, triggered actions, and a navigable menu.
- **🔄 Multifunctional Rotary Encoder**: High-precision volume control supporting short clicks (Mute), double clicks (Combo), and long clicks (opening the macro menu on the OLED).
- **🎛️ 3x3 Key Matrix**: Physcial mapping-corrected 9-key matrix with support for normal macros and advanced combos when used together with the encoder button.
- **🌐 Web Admin Console**: Integrated modern industrial dashboard to map buttons, manage flash storage files, and write/edit automation scripts in **DuckyScript** directly from your browser.
- **🏡 Home Assistant Automation**: Custom `HA_TOGGLE` command to trigger smart home switches, lights, and scenes directly on the local network without relying on a PC.
- **🔌 Native USB Emulation**: Uses the TinyUSB stack to act as a native USB HID device (keyboard/media keys) and serial CDC port simultaneously.
- **💓 Haptic Feedback (Vibration)**: Vibration manager with 5 distinct haptic patterns for physical events (normal click, mute, macro error, knob rotation, etc.).
- **🌈 NeoPixel RGB Status**: Real-time visual feedback indicating Python bridge connection status, mute status, and macro execution state.

---

## 🛠️ Pinout Specifications (Hardware)

| Component | ESP32-S3 Pin | Function |
| :--- | :--- | :--- |
| **Rotary Encoder CLK/DT** | `GPIO 1` & `GPIO 2` | Knob rotation reading |
| **Rotary Encoder SW (Button)**| `GPIO 4` | Short/long clicks and combo mode modifier |
| **Buzzer** | `GPIO 10` | Boot sounds and action audio cues |
| **Vibration Motor (Haptic)** | `GPIO 18` | Haptic feedback |
| **NeoPixel RGB LED** | `GPIO 48` | Colorful visual status |
| **Matrix Rows** | `GPIO 7`, `6`, `5` | Physical key scanning rows |
| **Matrix Columns** | `GPIO 15`, `16`, `17` | Physical key scanning columns |
| **I2C SDA / SCL** | `GPIO 8` / `GPIO 9` | OLED display connection |

---

## 📁 File Structure

* `MynouPAD.cpp`: Main firmware source code (C++/Arduino).
* `secrets.h`: Contains sensitive credentials (Wi-Fi and API tokens). **(Ignored by Git for security)**.
* `secrets.h.example`: Template for public configuration.
* `.gitignore`: Configured to exclude build folders, IDE directories, and `secrets.h`.

---

## ⚙️ Setup and Compilation

### 1. Create the Credentials File
Copy the `src/secrets.h.example` file to `src/secrets.h` and fill in your credentials:

```cpp
// src/secrets.h
#ifndef SECRETS_H
#define SECRETS_H

#define SECRET_WIFI_SSID     "YOUR_WIFI_SSID"
#define SECRET_WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#define SECRET_HA_TOKEN      "Bearer YOUR_HOME_ASSISTANT_TOKEN"

#endif
```

### 2. Arduino IDE Settings (If applicable)
* **Board**: ESP32S3 Dev Module
* **USB CDC On Boot**: Enabled
* **USB Mode**: TinyUSB
* **Partition Scheme**: 16M Flash (3MB APP / 9.9MB FATFS)

### 3. PlatformIO Settings
The `platformio.ini` file should be configured to use the appropriate USB stack and the 16MB custom partition.

---

## ✍️ Supported DuckyScript Syntax

The native MynouPAD engine supports the following commands in the scripts stored in Flash:

* `STRING <text>`: Types the specified text.
* `DELAY <milliseconds>`: Pauses execution for the defined time.
* `ENTER`, `TAB`, `ESC`: Presses the respective special keys.
* `GUI` or `GUI <key>`: Presses the Windows/Command key (alone or combined with another).
* `CTRL <key>` or `CONTROL <key>`: Presses the Control key combined with another.
* `HA_TOGGLE <entity_id>`: **(Custom)** Toggles the state of a device in Home Assistant directly via local API. Example:
  ```text
  HA_TOGGLE light.office_desk_lamp
  ```

---

Developed with ☕ and 💻 by [Mynoush](https://github.com/Mynoush).
