

# PYC Race Mark - LoRa Buoy Position Sender

## Project Info

* **Name:** RaceMarks - LoRa Buoy Position Sender
* **Description:** GPS-equipped buoy transmitter that sends position data via LoRa for race mark tracking
* **Goal:** Transmit buoy GPS coordinates at timed intervals with nav light control

## Requirements

### Master State Timing Table

| UTC Start | UTC End | GPS Sampling                                | LORA Sending | Low Power | Nav Light                             |
| --------- | ------- | ------------------------------------------- | ------------ | --------- | ------------------------------------- |
| 0:00      | 1:00    | 15-second intervals (0, 15, 30, 45 seconds) | 30 seconds   | No        | Off                                   |
| 1:00      | 4:00    | 15-second intervals (0, 15, 30, 45 seconds) | 30 seconds   | No        | Interval 10 second, duration 1 second |
| 4:00      | 13:00   | Single GPS reading (no accumulation)        | 30 minutes   | No        | Interval 10 second, duration 1 second |
| 13:00     | 22:00   | Single GPS reading (no accumulation)        | 30 minutes   | Yes       | Off                                   |
| 22:00     | 23:59   | 15-second intervals (0, 15, 30, 45 seconds) | 30 Seconds   | No        | Off                                   |

- Only transmit when GPS has a valid fix (non-zero lat/long)
- Device ID = 6

* Mirror transmissions to serial port for debugging
* During transmission, turn on RGB LED blue
* Nav light on GPIO6
* Lower Power Mode: shut down GPS and LoRa Radio

#### Display

* Initialize display with: `SSD1306Wire displayBd(0x3c, 500000, SDA, SCL, GEOMETRY_128_64, GPIO10);`
* Show "GPS initing..." centered on startup (ArialMT\_Plain\_16)
* Show boot wait counter during GPS acquisition (ArialMT\_Plain\_10)
* Show transmitted message string on successful send
* Turn off display after 5 minutes from restart

#### Power Management

* Vext controls external power to GPS and display (LOW = ON, HIGH = OFF)
* Battery voltage read via ADC with VBAT\_ADC\_CTL gate
* Formula: VBAT = 2 * V(ADC1)
* Low power mode (13:00-22:00): GPS and radio powered off, CPU sleeps 28 min between transmits; GPS restarts on wake via stWaitGPSBoot

## State Machine

| State | Description |
|-|-|
| stWaitGPSBoot | Wait for GPS module to become available, attach 1PPS interrupt |
| stWaitGPS1PPS | Idle, waiting for GPS 1-second pulse interrupt |
| st1SecPulseGrapGPSData | Read GPS data, decide next action based on second counter |
| stReadGPSSave2Buffer | Read GPS and save to buffer (at 15/45 second marks) |
| stReadGPSSave2BufferAndTransmit | Read GPS, save to buffer, transition to transmit (at 0/30 second marks) |
| stTxString | Wait for assigned time slot, send LoRa packet, flash nav light |
| stSleeping | Low power mode (13:00-22:00): power off GPS and radio, sleep CPU for 28 min |

## LoRa Message Format

```
|<msg_type>,<device_id>,<HHMMSS>,<lat>,<long>,<battery>
```

Example: `|2,3,183045,41.234567,-72.123456,3850`

| Field | Description |
|-|-|
| msg\_type | 2 = Buoy message |
| device\_id | Device ID |
| HHMMSS | GPS time (hours, minutes, seconds) |
| lat | Latitude (integer.fractional) |
| long | Longitude (integer.fractional) |
| battery | VBAT |

Messages are concatenated in the buffer - up to 2 position readings per transmit cycle.

## Time Slot Configuration

| Parameter | Value | Description |
|-|-|-|
| TXTENTHSSLOT | 6 | Transmit delay = 600ms after second mark (collision avoidance) |

## LoRa Radio Configuration

| Parameter | Value |
|-|-|
| Frequency | 915 MHz |
| TX Power | 14 dBm |
| Bandwidth | 500 kHz |
| Spreading Factor | SF8 |
| Coding Rate | 4/6 |
| Preamble Length | 8 symbols |
| IQ Inversion | Off |
| TX Timeout | 3000 ms |

## Hardware Used

| Component | Pin | Notes |
|-|-|-|
| RGB LED | GPIO13 | Onboard, status indicator during TX |
| GPS Module | AIR530Z | Via GPS\_Air530Z library, 57600 baud |
| GPS 1PPS | GPIO12 | 1-second pulse interrupt (INPUT\_PULLUP) |
| Nav Light | GPIO6 | External LED/light (OUTPUT\_PULLDOWN) |
| Solar Panel | ADC2 | Light level / voltage sensing (INPUT) |
| OLED Display | SDA/SCL | 0.96" 128x64, I2C address 0x3c |
| Battery ADC | ADC | Via VBAT\_ADC\_CTL gate |

## Board Specs

| Spec | Value |
|-|-|
| MCU | ASR6502 (ARM Cortex M0+ @ 48MHz) |
| LoRa Chip | SX1262 |
| GPS Module | AIR530Z |
| Display | 0.96" OLED 128x64 (I2C) |
| Flash | 128KB |
| SRAM | 16KB |

## Development Environment

* **IDE:** Arduino IDE
* **Board Package:** Heltec CubeCell (via Board Manager)
* **Board URL:** `https://github.com/HelTecAutomation/CubeCell-Arduino`

## Libraries Needed

| Library | Purpose |
|-|-|
| LoRaWan\_APP.h | CubeCell hardware, LoRa radio, RGB LED |
| GPS\_Air530Z.h | GPS module communication |
| HT\_SSD1306Wire.h | Onboard OLED display |
| Wire.h | I2C communication |

## Code Structure

```
/RaceMarks
  LoRaBuoySend.ino   - Main buoy transmitter sketch
  project_outline.md  - This document
```

GitHub site: https://github.com/SailorScott/LoRaBuoySend.git

## Not Yet Implemented

* [ ] Thursday night race schedule detection (day-of-week check exists but not wired in)
* [ ] Variable transmission intervals (race vs non-race)
* [ ] Base station race time check on 10-minute intervals
* [x] Display auto-off after 5 minutes
* [x] Low power sleep 13:00-22:00 (GPS + radio off, CPU sleeps 28 min)
* [ ] Auto-sleep after 5 hours (BOATAUTOSLEETIME defined but unused)

## Notes

* `getDayOfWeek()` uses Zeller's Congruence but has a bug: `month % 3` should be `month < 3` for correct January/February handling
* GPS is set to GPS + GLONASS dual mode
* `fracPart()` helper extracts decimal portion of GPS coordinates to 6 decimal places
* Transmit buffer accumulates multiple readings separated by `|` delimiter

## References

* [Heltec CubeCell Docs](https://heltec.org)
