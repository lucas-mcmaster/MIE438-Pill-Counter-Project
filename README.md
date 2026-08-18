# Embedded Automatic Pill Counter & Sorter

![Language](https://img.shields.io/badge/Language-C-00599C?logo=c)
![MCU](https://img.shields.io/badge/MCU-STM32H753ZI%20(ARM%20Cortex--M7)-03234B?logo=stmicroelectronics)
![IDE](https://img.shields.io/badge/Toolchain-STM32CubeIDE%20%7C%20HAL-03234B)
![Architecture](https://img.shields.io/badge/Architecture-Non--Blocking%20FSM-brightgreen)
![Accuracy](https://img.shields.io/badge/Counting%20Accuracy-100%25-success)

> **MIE438 Microprocessors and Microcontrollers | University of Toronto**  
> *Authors: Lucas McMaster, Ahmed Fahmi, Anthony Sergnese, Joy Mehany*  
> *Project Video Demonstration:* [YouTube Link](https://youtu.be/53f0EF_NOaQ)

---

## Project Overview

Manual prescription pill counting in independent pharmacies is slow, labour-intensive, and prone to human error. Commercial optical counters are often cost-prohibitive for small clinics. 

This project implements a low-cost, automated, embedded pill counting device using the **STM32H753ZI Nucleo** (ARM Cortex-M7). The system combines precision mechanical singulation with an interrupt-driven, non-blocking firmware architecture to achieve automated dispensing, real-time optical pill detection, and bidirectional user interfacing.

---

## Mechanical & Hardware Architecture

```text
                +---------------------------------------+
                |        12V 2A DC Power Supply         |
                +---------------------------------------+
                     |                             |
                     v                             v
       +---------------------------+   +-----------------------+
       |    L298N Motor Driver     |   |   5V Step-Down Rail   |
       +---------------------------+   +-----------------------+
                     |                             |
                     v                             v
       +---------------------------+   +-----------------------+
       | NEMA 17 Stepper (~30 RPM) |   |  STM32H753ZI Nucleo   |
       | (Rotates Disc Mechanism)  |   +-----------------------+
       +---------------------------+       |        |        |
                     |                     |        |        |
                     v                     v        v        v
       +---------------------------+   [EXTI]    [I2C]    [TIM]
       | Single-File Gravity Chute |  IR Beam     LCD    Encoder
       +---------------------------+   Sensor    Display (KY-040)
```

1. **Mechanical Singulation**:
   - A 3D-printed rotating disc driven by a NEMA 17 stepper motor at ~30 RPM uses centrifugal force and stationary radial diverter blades to guide Size 00 capsules into a perimeter single-file track.
   - The chute entry width ($12.15\text{ mm}$) permits only one capsule ($7.00\text{ mm}$) to pass at a time, mechanically deflecting overlapping capsules back onto the rotating platter.
2. **Optical Break-Beam Sensing**:
   - An Adafruit IR break-beam sensor spans the drop chute, triggering hardware external interrupts upon capsule passage.
   - Custom optical aperture engineering: Fitted with a $2\text{ mm}$ pinhole aperture mask to restrict beam divergence, eliminating false negatives on small capsules.
3. **Power Distribution**:
   - Dual-rail topology: 12V rail drives the L298N H-Bridge motor driver; the regulated 5V driver output powers the STM32 Nucleo, IR sensor, and 16x2 LCD display.
   - External $4.7\text{ k}\Omega$ pull-up resistors installed on the I2C bus (PB8/PB9) to ensure fast rise-time compliance and bus stability.

---

## Firmware Architecture & Software Design

The firmware is written in bare-metal C using the STM32 Hardware Abstraction Layer (HAL). It operates as a deterministic, **non-blocking superloop** governed by a 4-state **Finite State Machine (FSM)**:

```text
              +-----------------------------------+
              |           Initialization          |
              |  (HAL, GPIO, I2C, Timers, EXTI)   |
              +-----------------------------------+
                                |
                                v
                        +---------------+
              +-------->|  STATE_IDLE   |<--------+
              |         +---------------+         |
              |                 |                 |
              | (Target = 0)    | (Target > 0)    | (Reset Ack)
              v                 v                 |
      +---------------+ +---------------+         |
      | STATE_RUNNING | | STATE_RUNNING |         |
      | (Inventory)   | | (Target Count)|         |
      +---------------+ +---------------+         |
              \                 /                 |
   (10s Timeout | Target Met)   |                 |
                v               v                 |
              +-------------------+               |
              |  STATE_COMPLETE   |---------------+
              +-------------------+
```

### Key Modules:
- **`SystemConfig.c` / `SystemConfig.h`**: Manages master FSM states, transition logic, operational modes, and the 10-second empty-hopper safety timeout.
- **`BeamSensorHandler.c` / `BeamSensorHandler.h`**: Manages the EXTI falling-edge interrupt from the IR break-beam sensor. Implements a $75\text{ ms}$ non-blocking software debounce filter using `HAL_GetTick()` to eliminate double-counting caused by pill tumbling.
- **`MotorDriver.c` / `MotorDriver.h`**: Executes non-blocking 4-step bipolar stepper phase sequencing with a $10\text{ ms}$ step delay. Instantly halts rotation by setting all driver inputs to LOW upon state exit.
- **`UserInterface.c` / `UserInterface.h`**:
  - Custom 4-bit nibble I2C LCD driver for the PCF8574 expander without third-party library overhead.
  - Quadrature Rotary Encoder decoding via hardware timer in Encoder Mode (`TIM_ENCODERMODE_TI12`) using `__HAL_TIM_GET_COUNTER()`, offloading CPU processing.
  - Push-button input handled via EXTI with a $350\text{ ms}$ software debounce filter.
  - Atomic variable snapshotting to prevent display race conditions during interrupt firing.

---

## Source Code Structure

```text
MIE438-Pill-Counter-Project/
├── Core/
│   ├── Inc/
│   │   ├── BeamSensorHandler.h   # Optical break-beam sensor EXTI prototypes
│   │   ├── MotorDriver.h         # Stepper motor 4-step bipolar driver definitions
│   │   ├── SystemConfig.h        # Finite State Machine & system state definitions
│   │   ├── UserInterface.h       # I2C LCD driver & rotary encoder prototypes
│   │   └── main.h                # Pin definitions and global macros
│   └── Src/
│       ├── BeamSensorHandler.c   # Interrupt handler with 75ms debounce filter
│       ├── MotorDriver.c         # Non-blocking stepper motor commutation logic
│       ├── SystemConfig.c        # State processing, timeout, & mode handling
│       ├── UserInterface.c       # Low-level I2C LCD commands & snapshotting
│       ├── main.c                # Main non-blocking superloop execution
│       ├── gpio.c / i2c.c / tim.c # STM32CubeMX generated peripheral setup
│       └── stm32h7xx_it.c        # Hardware interrupt vector handlers
├── PillCountingProject.ioc       # STM32CubeMX hardware configuration file
└── README.md
```

---

## Validation & Test Results

- **Counting Accuracy**: Achieved **100% accuracy** across 30 independent verification trials (10 trials each at 25, 50, and 100 pill targets) under controlled singulation.
- **Inventory Mode Timeout**: Successfully transitioned to `STATE_COMPLETE` within $10.0 \pm 0.2\text{ s}$ of hopper emptying across 5 validation trials.
- **Debounce Optimization**:
  - Break-Beam Sensor: Increased from $20\text{ ms}$ to $75\text{ ms}$ to eliminate multiple triggers from pill surface facets.
  - Encoder Button: Increased from $350\text{ ms}$ to $350\text{ ms}$ to eliminate contact chatter skips from `STATE_IDLE` to `STATE_COMPLETE`.

---

## Build & Flashing Instructions

1. Open **STM32CubeIDE** and import the project:
   `File` $\to$ `Open Projects from File System...` $\to$ Select repository folder.
2. Build the project using `Ctrl+B` (or hammer icon).
3. Connect the STM32H753ZI Nucleo board via Micro-USB.
4. Flash firmware: `Run` $\to$ `Debug` (or `Run`).
