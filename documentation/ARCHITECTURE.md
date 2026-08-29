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
