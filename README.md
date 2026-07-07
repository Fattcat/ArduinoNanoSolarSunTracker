### ArduinoNanoSolarSunTracker

# Advanced Dual-Axis Sun Tracker v3.0

An optimized, event-driven, non-blocking dual-axis solar tracking system implemented in Arduino C++. The system uses four Light Dependent Resistors (LDR) arranged in a quadrant matrix to dynamically track the sun's position using two servo motors (Azimuth/Horizontal and Elevation/Vertical). 

It features an **Exponential Moving Average (EMA)** digital low-pass filter to eliminate sensor noise, hard-coded mechanical safety limits, and a built-in **Serial Command Line Interface (CLI)** for real-time telemetry and manual overriding.

---

## 🚀 Features

*   **Non-Blocking Architecture:** Built entirely without `delay()`, utilizing a `millis()` hardware-timer state machine. The system stays highly responsive to serial interface interrupts.
*   **Signal Processing (EMA Filter):** Implements a digital low-pass filter to smooth out ambient light fluctuations and high-frequency noise.
*   **Asynchronous Serial CLI:** A robust ASCII command parser operating at 115200 baud to query system states, pause automation, and manually drive the servos with active boundary validation.
*   **Safety Constraints:** Hardware-level protection via `constrain()` boundaries to prevent physical collisions, binding, or cable strain.

---

## 🔌 Hardware Connections (Wiring)

### 1. LDR Sensor Matrix (Voltage Dividers)
To read analog voltage changes from the LDRs, each sensor must be configured as a voltage divider using a static $10\text{ k}\Omega$ pull-down resistor. The output voltage is calculated as:

$$V_{out} = V_{cc} \cdot \frac{R_{static}}{R_{LDR} + R_{static}}$$

| Sensor Position | Arduino Analog Pin | Pulldown Resistor | Connection Logic |
| :--- | :--- | :--- | :--- |
| **Top Left (TL)** | `A0` | $10\text{ k}\Omega$ | $5V \rightarrow LDR \rightarrow A0 \rightarrow 10\text{ k}\Omega \rightarrow GND$ |
| **Top Right (TR)** | `A1` | $10\text{ k}\Omega$ | $5V \rightarrow LDR \rightarrow A1 \rightarrow 10\text{ k}\Omega \rightarrow GND$ |
| **Bottom Left (BL)** | `A2` | $10\text{ k}\Omega$ | $5V \rightarrow LDR \rightarrow A2 \rightarrow 10\text{ k}\Omega \rightarrow GND$ |
| **Bottom Right (BR)** | `A3` | $10\text{ k}\Omega$ | $5V \rightarrow LDR \rightarrow A3 \rightarrow 10\text{ k}\Omega \rightarrow GND$ |

### 2. Servo Actuators

| Actuator | Arduino PWM Pin | Operating Range | Power Source |
| :--- | :--- | :--- | :--- |
| **Horizontal Servo (X-Axis)** | `D9` | $10^\circ \text{ to } 170^\circ$ | External 5V PSU |
| **Vertical Servo (Y-Axis)** | `D10` | $20^\circ \text{ to } 160^\circ$ | External 5V PSU |

> ⚠️ **CRITICAL POWER NOTE:** Do **NOT** power the servo motors directly from the Arduino 5V rail. Servos draw high transient currents under load, which will cause voltage sags, brownout resets, or damage to the onboard linear regulator. Use an external 5V (2A+) power supply and **ensure a common ground ($GND$)** is connected between the external PSU and the Arduino.

---

## 💻 Serial Command Interface (CLI)

Open your Serial Monitor, set the baud rate to **115200**, and configure line endings to **Newline (NL / `\n`)**. The parser is case-insensitive.

| Command | Description | System Response Example |
| :--- | :--- | :--- |
| `GetPos` | Queries current servo angles and operational mode. | `ACK GetPos H_Pos:90 V_Pos:90 State:AUTO_TRACKING` |
| `Start` | Enables automatic LDR-based solar tracking. | `ACK Start - Automatic tracking enabled.` |
| `Pause` | Suspends automatic tracking; enables manual override mode. | `ACK Pause - Automatic tracking suspended.` |
| `MoveHome` | Forces both axes into the safe home position ($90^\circ$). | `ACK MoveHome - Safe center position executed.` |
| `MoveX <deg>` | Drives X-axis to specific angle. **Forces Auto-Pause.** | `ACK MoveX: 120 - Auto-tracking PAUSED.` |
| `MoveY <deg>` | Drives Y-axis to specific angle. **Forces Auto-Pause.** | `ACK MoveY: 45 - Auto-tracking PAUSED.` |

### Input Bound Validation Example:
If you send `MoveX 200`, the system validates the input against the safety parameters ($10^\circ - 170^\circ$), truncates the execution safely, and replies with a warning:
`ACK MoveX: 170 (Warning: Constrained from 200) - Auto-tracking PAUSED.`

---

## 🛠️ Mechanical Assembly Notes

1.  **Shading Divider (The Cross):** Place an opaque, weather-resistant $X$-shaped or $+$-shaped divider between the four LDR sensors. Proper tracking resolution depends entirely on the shadow cast by this divider when the sun moves off-center.
2.  **Structural Balancing:** Center the solar panel's center of mass directly over the rotation shafts to minimize torque requirements and protect servo gearboxes from radial load wear.
