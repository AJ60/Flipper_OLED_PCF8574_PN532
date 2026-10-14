# 🏛️ System & Firmware Architecture: DIY Flipper Zero (OLED Edition)

**Target MCU**: STM32WB55CGU6 (ARM Cortex-M4 @ 64 MHz + ARM Cortex-M0+ @ 32 MHz)  
**RTOS**: FreeRTOS Kernel v10.5.1  
**Framework**: Furi OS & Furi HAL  
**Maintainer**: [**AJ_60**](https://github.com/AJ60)

---

## 1. High-Level Multi-Layer System Stack

The firmware is structured into modular layers designed for deterministic real-time execution, robust hardware abstraction, and event-driven application flow.

```mermaid
graph TD
    subgraph Layer5 [Layer 5: Application Layer]
        GUI_Apps[Main GUI Apps: NFC, SubGHz, RFID, Archive]
        FAP_Plugins[FAP Dynamic Applications & Plugins]
        CLI_Console[CLI Serial Console & RPC Engine]
    end

    subgraph Layer4 [Layer 4: System Services & Middleware]
        GUI_Srv[GUI Service / Canvas & Viewport Manager]
        Input_Srv[Input Service / Debounce & Event Dispatch]
        Storage_Srv[Storage Service / LittleFS & FatFS on SD]
        Power_Srv[Power Service / INA219 Battery Fuel Gauge]
        Dolphin_Srv[Dolphin Pet Engine & XP Gamification]
        Loader_Srv[Loader Service / Dynamic FAP Execution]
    end

    subgraph Layer3 [Layer 3: Furi Core OS]
        FuriPubSub[FuriPubSub Event Broker]
        FuriRecord[FuriRecord Registry & Service Locator]
        FuriThread[FuriThread / FreeRTOS Task Abstraction]
        FuriMutex[FuriMutex & FuriSemaphore Locking]
        FuriTimer[FuriTimer Software & Hardware Timers]
    end

    subgraph Layer2 [Layer 2: Furi Hardware Abstraction Layer - HAL]
        HAL_Display[furi_hal_display: SSD1306/SH1106 I2C Driver]
        HAL_Expander[furi_hal_pcf8574: I/O Expander & Haptic Driver]
        HAL_NFC[furi_hal_nfc: PN532 I2C & ST25R3916 SPI Stack]
        HAL_RFID[furi_hal_rfid: 125kHz Analog Timer Carrier & Demod]
        HAL_SubGHz[furi_hal_subghz: CC1101 SPI Transceiver]
        HAL_Power[furi_hal_power: INA219/INA226 I2C Monitor]
        HAL_BT[furi_hal_bt: IPCC Mailbox to Cortex-M0+ Coprocessor]
    end

    subgraph Layer1 [Layer 1: Hardware & Silicon]
        STM32WB55[STM32WB55CGU6 MCU: 1MB Flash / 256KB SRAM]
        I2C_Bus[I2C1 & I2C3 Hardware Controllers]
        SPI_Bus[SPI1 Hardware Controller]
        Timers_DMA[TIM1, TIM2, TIM16 & DMA1/DMA2 Channels]
        Peripherals[OLED Display, Buttons, PN532, CC1101, MicroSD, RFID Tank]
    end

    GUI_Apps --> GUI_Srv
    FAP_Plugins --> GUI_Srv
    CLI_Console --> FuriRecord
    GUI_Srv --> FuriPubSub
    Input_Srv --> FuriPubSub
    FuriPubSub --> FuriThread
    FuriThread --> FreeRTOS[FreeRTOS Kernel]
    FreeRTOS --> HAL_Display
    FreeRTOS --> HAL_Expander
    FreeRTOS --> HAL_NFC
    FreeRTOS --> HAL_RFID
    FreeRTOS --> HAL_SubGHz
    FreeRTOS --> HAL_Power
    FreeRTOS --> HAL_BT
    HAL_Display --> I2C_Bus
    HAL_Expander --> I2C_Bus
    HAL_Power --> I2C_Bus
    HAL_NFC --> I2C_Bus
    HAL_SubGHz --> SPI_Bus
    HAL_RFID --> Timers_DMA
    HAL_BT --> STM32WB55
```

---

## 2. Dual-Core Inter-Processor Communication (IPC)

The STM32WB55 uses an asymmetric dual-core architecture:
1. **CPU1 (ARM Cortex-M4 @ 64 MHz)**: Runs the main user application, GUI rendering, FreeRTOS kernel, file system, and peripheral drivers.
2. **CPU2 (ARM Cortex-M0+ @ 32 MHz)**: Dedicated RF coprocessor running STMicroelectronics proprietary BLE stack and 2.4 GHz physical layer.

```mermaid
sequenceDiagram
    autonumber
    participant App as Application / FuriHAL
    participant M4 as CPU1 (Cortex-M4 Main OS)
    participant IPCC as IPCC Hardware Mailbox
    participant RAM as Shared SRAM2 Memory
    participant M0 as CPU2 (Cortex-M0+ BLE Stack)

    App->>M4: furi_hal_bt_init()
    M4->>RAM: Write BLE Command Buffer (HCI Packet)
    M4->>IPCC: Signal Channel 1 Interrupt (IPCC_TX)
    IPCC-->>M0: Trigger IPCC RX Interrupt on CPU2
    M0->>RAM: Parse HCI Command from Shared RAM
    M0->>M0: Execute Radio / BLE Synthesis
    M0->>RAM: Write Response & Status
    M0->>IPCC: Signal Channel 1 Status (IPCC_RX)
    IPCC-->>M4: Trigger IPCC TX Acknowledge on CPU1
    M4->>App: Callback / Data Available Notification
```

---

## 3. FreeRTOS Task Scheduling & Priority Matrix

Each system service operates in its own deterministic thread context with configured priorities:

| Task Name | Priority | Stack Size | Role / Description |
|---|---|---|---|
| **`GuiService`** | `FuriPriorityNormal` (24) | 4096 bytes | Dispatches canvas drawing commands, manages window viewports, flushes display buffer. |
| **`InputService`** | `FuriPriorityHigh` (36) | 2048 bytes | Handles PCF8574 interrupt lines (`PB0`), samples keypad state, performs 2-tick debouncing. |
| **`StorageService`** | `FuriPriorityBelowNormal` (16) | 4096 bytes | Handles SD card FatFS filesystem access over SPI1 bus with file mutex locking. |
| **`PowerService`** | `FuriPriorityBelowNormal` (16) | 2048 bytes | Queries INA219/INA226 voltage/current, calculates battery curves, handles low-power alerts. |
| **`NotificationService`** | `FuriPriorityLow` (12) | 1536 bytes | Manages vibration motor rumbling, LED backlights, and speaker tone sequences. |
| **`BtService`** | `FuriPriorityHigh` (36) | 2048 bytes | Manages Bluetooth Low Energy pairing, serial over BLE (CDC), and remote control. |
| **`LoaderService`** | `FuriPriorityNormal` (24) | 4096 bytes | Loads and dynamically links `.fap` application binaries from the microSD card into SRAM. |

---

## 4. Furi Core OS Primitive Model

The Furi OS framework provides thread-safe abstractions over low-level RTOS constructs:

1. **`FuriPubSub`**: Single-publisher, multi-subscriber event pipeline used for asynchronous decoupled messages (e.g. `InputEvent`, `BatteryEvent`).
2. **`FuriRecord`**: Thread-safe global service locator and dependency injection container (e.g. `furi_record_open(RECORD_GUI)`).
3. **`FuriMutex` / `FuriSemaphore`**: Mutual exclusion locks with priority inheritance to prevent priority inversions on shared hardware buses (I2C1, SPI1).
4. **`FuriThread`**: Encapsulates FreeRTOS tasks with memory-safe initialization, lifecycle callbacks, and signal flag mechanisms.

---

## 5. Advanced Firmware Features & System Enhancements (v2.3 Architecture)

The DIY Flipper firmware integrates specialized subsystem upgrades bridging **Unleashed**, **RogueMaster**, and custom hardware drivers:

### 🎮 A. Covert Stealth Mode ("Games Only" Profile)
* **Desktop Observer**: Located in [`desktop_view_main.c`](../applications/services/desktop/views/desktop_view_main.c). Listens for the Konami code (`▲ ▲ ▼ ▼ ◄ ► ◄ ►`) during main pet screen idle loop.
* **Haptic Actuation**: Drives PCF8574 I2C output pin 6 to vibrate the motor for 120 ms without producing audio buzzer noise.
* **Dynamic Menu Masking**: In [`loader_menu.c`](../applications/services/loader/loader_menu.c), the menu builder inspects `momentum_settings.games_only_mode`. When active, it filters out all penetration testing modules (`Sub-GHz`, `125 kHz RFID`, `NFC`, `Infrared`, `iButton`, `Bad USB`, and `GPIO`), presenting a benign retro-gaming interface.

### 🛡️ B. OLED Display Burn-In Mitigation & Contrast Scaling
* **Auto Sleep**: [`notification_app.c`](../applications/services/notification/notification_app.c) enforces a default 30-second screen-off timer (`display_off_delay_ms = 30000`).
* **Hardware Contrast Scaling**: [`furi_hal_light.c`](../targets/f7/furi_hal/furi_hal_light.c) maps incoming brightness levels linearly to SSD1306 contrast register `0x81` (from minimum `0x15` to maximum `0xCF`). When brightness is set to 0, it transmits `0xAE` (Display OFF / Sleep mode) over I2C1 to eliminate quiescent OLED current.

### 🐬 C. Dolphin Gamification & Animation Un-Gating
* **Deed Limits**: [`dolphin_deed.c`](../applications/services/dolphin/helpers/dolphin_deed.c) expands daily XP caps from 69 to 198 (RogueMaster standard), accelerating dolphin leveling across Sub-GHz and NFC actions.
* **Un-Gated Animations**: Sets `.unlock_anims = true` in Momentum settings defaults, enabling users to view and cycle through Level 1–3 animations without gating.

### 📻 D. Sub-GHz Visualizer & RF Analysis Ecosystem
* **ProtoView**: Real-time pulse visualizer with microsecond horizontal zoom and on-device decoding for TPMS, weather stations, and fixed-code remotes.
* **Ham Radio**: 70cm amateur radio receiver featuring real-time S0–S9+ signal meter, spectrum sweep graph, and persistent frequency plan stored at `/ext/ham_radio/band_plan.txt`.
* **Automatic Hardware Radio Routing**: Applications utilize `subghz_radio_device_loader` to automatically switch between onboard and external CC1101 transceivers with pin routing and RF switch coordination.

