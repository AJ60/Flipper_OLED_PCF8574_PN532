# 🌊 Firmware Boot & System Lifecycle: DIY Flipper Zero (OLED Edition)

**Target MCU**: STM32WB55CGU6  
**Maintainer**: [**AJ_60**](https://github.com/AJ60)

---

## 1. System Boot & Initialization Waterfall

The power-on sequence transitions from raw silicon reset through hardware validation, kernel activation, bus arbitration, and user-space service launch.

```mermaid
graph TD
    subgraph Phase1 [Phase 1: Silicon Reset & OTP Verification]
        PWR[Power-On / Reset Vector 0x08000000] --> RCC_Init[Configure RCC Clocks: HSE 32MHz PLL to 64MHz]
        RCC_Init --> OTP_Check{Check OTP @ 0x1FFF7000}
        OTP_Check -->|Valid Header 0xBABE| OTP_Load[Load Profile: Board Rev 12, Display MGG]
        OTP_Check -->|Missing / Corrupt| Fallback_Profile[Load Default Safe Parameters]
    end

    subgraph Phase2 [Phase 2: Hardware Low-Level Inits]
        OTP_Load --> GPIO_Init[Initialize GPIO Pinmux & Pull-ups]
        Fallback_Profile --> GPIO_Init
        GPIO_Init --> I2C1_Init[Initialize I2C1 Bus @ 400kHz PA9/PB9]
        I2C1_Init --> I2C3_Init[Initialize I2C3 Bus @ 100kHz PC0/PC1]
        I2C3_Init --> SPI1_Init[Initialize SPI1 Bus @ 32MHz PB3/PB5]
        SPI1_Init --> Timer_Init[Initialize Hardware Timers: TIM1, TIM2, TIM16]
    end

    subgraph Phase3 [Phase 3: Kernel & OS Startup]
        Timer_Init --> RTOS_Start[FreeRTOS Kernel Initialization]
        RTOS_Start --> Furi_Init[Furi Core Subsystems & Record Registry]
        Furi_Init --> Bus_Probe[Bus Hardware Autoprobing]
    end

    subgraph Phase4 [Phase 4: Hardware Probing & HAL Activation]
        Bus_Probe --> Probe_INA[Probe INA219 / INA226 Battery Fuel Gauge]
        Probe_INA --> Probe_OLED[Initialize SSD1306 OLED via I2C1 Command Stream]
        Probe_OLED --> Probe_PCF[Initialize PCF8574 IO Expander & Attach PB0 Ext Interrupt]
        Probe_PCF --> Probe_SD[Mount MicroSD FatFS Filesystem over SPI1]
    end

    subgraph Phase5 [Phase 5: System Services & GUI Launch]
        Probe_SD --> Spawn_Services[Spawn System Services in Dedicated Threads]
        Spawn_Services --> Srv_Input[Input Service]
        Spawn_Services --> Srv_Gui[GUI Service & Viewport Compositor]
        Spawn_Services --> Srv_Storage[Storage Service]
        Spawn_Services --> Srv_Power[Power Service]
        Spawn_Services --> Srv_BT[Bluetooth Low Energy Service]
        
        Srv_Gui --> Show_Splash[Render Splash Animation & Dolphin Engine]
        Show_Splash --> Main_Menu[Main Desktop & Applications Menu Ready]
    end
```

---

## 2. I2C Bus Arbitration & Rate-Limited Self-Healing Waterfall

Because the SSD1306 display, PCF8574 keypad, and INA219/INA226 power monitor share the single I2C1 bus, an arbitration and recovery mechanism ensures bus lockups never freeze the device.

```mermaid
sequenceDiagram
    autonumber
    participant App as Application / Render Loop
    participant Gui as GUI Service
    participant HAL as Furi HAL I2C1 Driver
    participant Bus as Physical I2C1 Bus
    participant PCF as PCF8574 Expander (0x20)
    participant OLED as SSD1306 Screen (0x3C)

    Note over Gui,OLED: Framebuffer Flush Cycle
    Gui->>HAL: furi_hal_i2c_acquire(I2C1)
    HAL->>Bus: Transmit OLED Frame Chunk (1024 bytes)
    Bus-->>OLED: Display Pixels Refreshed
    HAL->>Gui: furi_hal_i2c_release(I2C1)

    Note over PCF,HAL: Button Press Trigger (Active-Low)
    PCF-->>HAL: Pulls PB0 Low (Ext Interrupt)
    HAL->>HAL: Wake Input Thread (INPUT_THREAD_FLAG_ISR)
    HAL->>Bus: Read Expander Register (0x20)
    
    alt Normal Read Success
        Bus-->>HAL: Returns GPIO State (0xEF - Button Pressed)
        HAL->>App: Dispatches InputEvent (Key: OK, Type: Short)
    else I2C Read Failure / Bus Wedged by RF Noise
        Bus-->>HAL: Timeout / NACK / Corrupted Byte
        HAL->>HAL: Rate-Limited Self-Heal: furi_hal_pcf8574_check_and_restore()
        HAL->>Bus: Pulse SCL 9x Clocks to Clear Stuck Slaves
        HAL->>Bus: Send I2C STOP Condition
        HAL->>Bus: Re-assert Expander Config Mask
        HAL->>App: Retain Cached Button State (No Dropped Presses)
    end
```

---

## 3. Power State Transitions & Fast Wake Machine

The firmware supports intelligent power state transitions to maximize battery runtime:

```mermaid
stateDiagram-v2
    [*] --> Active_Run: Power On / Reboot

    state Active_Run {
        [*] --> High_Performance: CPU @ 64MHz, OLED 100% Brightness
        High_Performance --> Dimmed_Idle: No Input for 15s (OLED Contrast Dimmed)
    }

    state Low_Power_Sleep {
        CPU_Sleep: CPU Core WFI, OLED Off, Expander Active
    }

    state Deep_Stop_Mode {
        Stop_Mode: Clocks Gated, SRAM Retained, PB0 Interrupt Armed
    }

    Active_Run --> Low_Power_Sleep: Screen Timeout (30s inactivity)
    Low_Power_Sleep --> Active_Run: PB0 Button Interrupt / USB Plugged
    Low_Power_Sleep --> Deep_Stop_Mode: Sleep Timeout (10 minutes)
    Deep_Stop_Mode --> Active_Run: Physical Button Press on PB0 (Instant 2ms Wakeup)
```
