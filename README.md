<div align="center">

# 🖥️ ESP32 Smart PC Power Controller

**Control your gaming PC with Google Assistant, Alexa, Siri, or any smart home app — using a $1 relay and an ESP32.**

[![PlatformIO](https://img.shields.io/badge/Built%20with-PlatformIO-orange?logo=platformio)](https://platformio.org/)
[![Sinric Pro](https://img.shields.io/badge/Cloud-Sinric%20Pro-blue)](https://sinric.pro/)
[![ESP32](https://img.shields.io/badge/Hardware-ESP32-red)](https://www.espressif.com/en/products/socs/esp32)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

</div>

---

## 📑 Table of Contents
- [Features](#-features)
- [Hardware Required](#-hardware-required)
- [Wiring Diagram](#-wiring-diagram)
- [Getting Started (Setup Guide)](#-getting-started)
- [How It Works](#-how-it-works)
- [Usage Examples](#-usage-examples)
- [Customization](#-customization)

---

## ✨ Features

| Feature | Description |
|---|---|
| 🔵 **Google Assistant / Alexa / Siri** | "Hey Google / Siri, turn on my PC" |
| 📡 **Live Power State Detection** | Pings your PC every 5s to know if it's really ON or OFF |
| 🛡️ **Safety Overrides** | Blocks accidental shutdown and ignores power commands while booting (90s lockout) |
| ⚡ **Force Restart Kill-Switch** | Holds the power button 8s for a hardware-level reboot |
| 🔔 **Boot-Up Push Notification** | Receive a phone alert the moment your PC finishes booting |
| 💸 **Zero Extra Cost** | Uses your existing ESP32 + $1 relay — no new hardware |

---

## 🧰 Hardware Required

| Component | Notes |
|---|---|
| **ESP32 Dev Board** | Any standard 38-pin ESP32 module |
| **5V Single-Channel Relay Module** | Most common ~$1 relay boards work |
| **Female-to-Female Jumper Wires** | x3 (VCC, GND, IN) |
| **PC Motherboard** | Any ATX motherboard with a front-panel PWR_SW header |

> **No transistor, no level shifter, no soldering required!** This project uses a software "True Open-Drain" trick to drive a 5V relay from the ESP32's 3.3V GPIO safely.

---

## ⚡ Wiring Diagram

```
ESP32            Relay Module
------           ------------
VIN  ─────────► VCC
GND  ─────────► GND
D21  ─────────► IN
                COM ──┐
                NO  ──┘ ← Connect these two wires to your PC motherboard's PWR_SW header
```

> ⚠️ **Do NOT use D5 (GPIO 5).** It's an ESP32 strapping pin and will cause boot failures. Use D21, D22, D13, or D27 instead.

---

## 🚀 Getting Started

![Completed ESP32 and Relay wired to the motherboard](hardware-wiring.png)

**We've moved all hardware, wiring, Sinric Pro, and Firewall setup instructions to a dedicated guide.**

👉 **[Setup Guide & Tutorial](USER_GUIDE.md)** | 🚑 **[Troubleshooting & FAQ](USER_GUIDE.md#7-troubleshooting--faq)**

---

## 💡 How It Works

### High-Level System Architecture
Here is how the entire system communicates. The ESP32 acts as the bridge between the Cloud (Google/Alexa) and your physical hardware (the PC Case).

```mermaid
graph TD
    User([User]) -.->|Voice Command| VoiceApp[Google Home / Alexa]
    VoiceApp -.->|Cloud Sync| SinricPro[Sinric Pro Cloud]
    
    subgraph Local Network
        Router[Wi-Fi Router]
        ESP32[ESP32 Microcontroller]
        PC[Gaming PC]
        
        Router --- ESP32
        Router --- PC
    end
    
    SinricPro == WebSocket ==> Router
    ESP32 == ICMP Ping ==> PC

    ESP32 -- 5V Relay --> Case[PC Motherboard]
    
    classDef cloud fill:#2c3e50,stroke:#34495e,stroke-width:2px,color:#ecf0f1;
    classDef hardware fill:#27ae60,stroke:#2ecc71,stroke-width:2px,color:#fff;
    
    SinricPro:::cloud
    VoiceApp:::cloud
    ESP32:::hardware
    PC:::hardware
    Router:::hardware
```

### The "Open-Drain" Voltage Trick
Standard 5V relay modules don't fully turn off when driven by a 3.3V ESP32 GPIO — the relay gets "stuck ON". Instead of trying to output a HIGH voltage, this firmware toggles the pin between:
- **`OUTPUT LOW`** (0V) → Relay turns ON → PC power button pressed
- **`INPUT` (floating)** → Relay turns OFF via its own 5V pull-up → No voltage fight

```mermaid
stateDiagram-v2
    direction LR
    [*] --> Floating
    
    Floating --> Pulled_LOW : "Turn ON" Command
    note left of Floating
      ESP32 Pin = INPUT
      Relay is OFF
      (Safe / Invisible)
    end note
    
    Pulled_LOW --> Floating : 700ms Delay Ends
    note right of Pulled_LOW
      ESP32 Pin = OUTPUT LOW
      Relay is ON
      (Power Button Pressed)
    end note
```

### Digital Ping Power Sensing
Every 5 seconds, the ESP32 sends an ICMP ping to your PC's local IP. If the ping succeeds → PC is ON. If it times out → PC is OFF. This state is pushed to Sinric Pro, so your app always shows the real, live power status.

```mermaid
graph TD
    A[ESP32 Timer: 5 Seconds] --> B{Ping PC IP Address}
    B -- Success --> C[PC is ON]
    B -- Timeout --> D[PC is OFF]
    C --> E{State Changed?}
    D --> E
    E -- Yes --> F[Update Sinric Pro App]
    E -- No --> G[Wait 5 Seconds]
    F --> G
```

### Force Restart Logic
When you press the **Force Restart** switch, the relay holds the PC power button for **8 seconds**. This duration bypasses Windows ACPI and triggers the motherboard's hardware-level power cut — useful when the PC is completely frozen.

```mermaid
sequenceDiagram
    participant User
    participant SinricPro
    participant ESP32
    participant PC_Motherboard
    
    User->>SinricPro: "Force Restart PC"
    SinricPro->>ESP32: Trigger Force Switch
    ESP32->>PC_Motherboard: Pull PWR_SW LOW (ON)
    Note over ESP32,PC_Motherboard: Hold for 8000ms
    ESP32->>PC_Motherboard: Release PWR_SW (OFF)
    Note over PC_Motherboard: Hardware Power Cut Enforced
```

### Safety Overrides
To prevent you from accidentally turning off your PC while gaming, or turning it "ON" when it is already running (which would actually shut it down), the ESP32 intercepts commands and checks the real `<PC_IP>` state first. Furthermore, a **90-second Boot Lockout** prevents rapid, consecutive "Turn ON" requests while the PC is still starting up.

```mermaid
sequenceDiagram
    participant User
    participant SinricPro
    participant ESP32
    participant Relay
    participant PC_IP
    
    User->>SinricPro: "Turn ON my PC"
    SinricPro->>ESP32: ON Command
    ESP32->>PC_IP: ICMP Ping
    
    alt PC is already ON (Ping Success)
        PC_IP-->>ESP32: Reply
        note over ESP32: Command Ignored (Already running)
        ESP32-->>SinricPro: State remains ON
    else PC is OFF (Ping Timeout)
        PC_IP--xESP32: Timeout
        alt Boot Lockout Timer Active (< 90s)
            note over ESP32: Command Ignored (PC is booting)
            ESP32-->>SinricPro: State remains ON
        else Boot Timer Inactive
            note over ESP32: Safe to proceed
            ESP32->>Relay: Pulse 700ms
            note over ESP32: Start 90s Boot Lockout Timer
            ESP32-->>SinricPro: State changed to ON
        end
    end
```

---

## 📱 Usage Examples

| You Say | Platform | What Happens |
|---|---|---|
| *"Hey Google, turn on my PC"* | Google Assistant | ESP32 pings PC. If OFF → relay pulses 700ms. If already ON → ignores. |
| *"Alexa, turn on Gaming PC"* | Amazon Alexa | Same safety logic applies. |
| *"Hey Siri, turn on my PC"* | Apple Shortcuts | Triggers the Sinric Pro shortcut action. Same safety logic. |
| *"Hey Google, turn off my PC"* | Google Assistant | ESP32 pings PC. If ON → relay pulses 700ms (graceful shutdown). |
| *"Hey Google, hard reset my PC"* | Google Assistant | Force Restart switch → relay holds for 8s → hardware kill |
| *(PC boots)* | Any | Pings detect OFF→ON change → push notification fires on your phone |

---

## 🔧 Customization

| Setting | Location | Default |
|---|---|---|
| Relay pulse duration | `src/main.cpp` → `triggerRelay()` | `700ms` |
| Force Restart duration | `src/main.cpp` → `triggerRelayForce()` | `8000ms` |
| Ping interval | `src/main.cpp` → `PING_INTERVAL` | `5000ms` |
| Relay GPIO pin | `src/config.h` → `RELAY_PIN` | `21` |

---

## 🏗️ Project Structure

```
esp32-sinricpro-smart-pc-power/
├── src/
│   ├── main.cpp              # Main firmware logic
│   ├── config.example.h      # Configuration template (copy → config.h)
│   └── config.h              # ← Your credentials (gitignored, never committed)
├── platformio.ini            # PlatformIO board config and library dependencies
├── .gitignore
└── README.md
```

---

## 📦 Libraries Used

| Library | Purpose |
|---|---|
| [SinricPro](https://github.com/sinricpro/esp8266-esp32-sdk) | Cloud control via Alexa/Google |
| [ESP32Ping](https://github.com/marian-craciunescu/ESP32Ping) | ICMP ping for power state detection |
| [ArduinoJson](https://arduinojson.org/) | JSON parsing for Sinric Pro |
| [WebSockets](https://github.com/Links2004/arduinoWebSockets) | WebSocket transport for Sinric Pro |

---

## 🤝 Contributing

Pull requests are welcome! For major changes, please open an issue first.

---

## 📄 License

MIT © [Chetan Goswami](https://github.com/chetangoswami)
