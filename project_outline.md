# PYC Race Mark - LoRa Buoy Position Sender

## Project Info

* **Name:** RaceMarks - LoRa Buoy Position Sender
* **Description:** GPS-equipped buoy transmitter that sends position data via LoRa for race mark tracking
* **Goal:** Transmit buoy GPS coordinates at timed intervals with nav light control

## Requirements

#### GPS Position Transmission

1. Read GPS position at 15-second intervals (0, 15, 30, 45 seconds)
   1. Write to serial port

2. Transmit position via LoRa at 30-second intervals (0 and 30 seconds)
3. Buffer up to 2 GPS positions per transmission cycle
4. Only transmit when GPS has a valid fix (non-zero lat/long)

#### Transmission Scheduling

1. Between UTC 2200 and 0400 send position every 30 seconds
2. Between UTC 0401 and 2159 send position every 5 minutes
3. Device ID = 6
4. Mirror to serial port for debugging.
5. During transmission, turn on RGB LED so it is Blue. 

#### Navigation Light

1. Navigation light on GPIO6
2. Navigation light is operational from UTC 0100 to 1300.
   1. Use GPS time

3. Navigation light turns on every 20 seconds, and stays on for 0.5 seconds.

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
* Future: Normal sleep mode with GPS off when idle

## State Machine

| State | Description |
|-|-|
| stWaitGPSBoot | Wait for GPS module to become available, attach 1PPS interrupt |
| stWaitGPS1PPS | Idle, waiting for GPS 1-second pulse interrupt |
| st1SecPulseGrapGPSData | Read GPS data, decide next action based on second counter |
| stReadGPSSave2Buffer | Read GPS and save to buffer (at 15/45 second marks) |
| stReadGPSSave2BufferAndTransmit | Read GPS, save to buffer, transition to transmit (at 0/30 second marks) |
| stTxString | Wait for assigned time slot, send LoRa packet, flash nav light |

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
| TXSECTIMETRIG | 30 | Transmit at 0 and 30 seconds |
| GPSREADSECTIMETRIG | 15 | Read GPS at 0, 15, 30, 45 seconds |
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
* [ ] Display auto-off after 5 minutes
* [ ] Sleep mode with GPS power-down
* [ ] Auto-sleep after 5 hours (BOATAUTOSLEETIME defined but unused)

## Notes

* `getDayOfWeek()` uses Zeller's Congruence but has a bug: `month % 3` should be `month < 3` for correct January/February handling
* GPS is set to GPS + GLONASS dual mode
* `fracPart()` helper extracts decimal portion of GPS coordinates to 6 decimal places
* Transmit buffer accumulates multiple readings separated by `|` delimiter

## References

* [Heltec CubeCell Docs](https://heltec.org)
