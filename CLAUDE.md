# RaceMarks - LoRa Buoy Position Sender

GPS-equipped buoy transmitter that sends position data via LoRa for race mark tracking.

## Project Structure
- `project_outline.md` — Full spec, state machine, LoRa config, hardware details
- `LoRaBuoySend/LoRaBuoySend.ino` — Main buoy transmitter sketch

## Hardware
- Board: CubeCell-GPS (HTCC-AB02S) with ASR6502, SX1262 LoRa, AIR530Z GPS
- FQBN: `CubeCell:CubeCell:CubeCell-GPS`
- CP210x USB on `/dev/ttyUSB0`
- Device ID: 6

## Build & Upload
```bash
# Compile
arduino-cli compile --fqbn CubeCell:CubeCell:CubeCell-GPS LoRaBuoySend

# Upload
arduino-cli upload --fqbn CubeCell:CubeCell:CubeCell-GPS --port /dev/ttyUSB0 LoRaBuoySend

# Serial monitor (115200 baud)
stty -F /dev/ttyUSB0 115200 raw -echo && cat /dev/ttyUSB0
```

## WSL2 USB Setup
CubeCell must be attached via usbipd from Windows PowerShell (admin):
```powershell
usbipd list
usbipd bind --busid <BUSID>
usbipd attach --wsl --busid <BUSID>
```

## Transmit Schedule
| UTC       | TX Interval | GPS Sampling  | Low Power        | Nav Light |
|-----------|-------------|---------------|------------------|-----------|
| 00:00-04:00 | 30 sec    | Every 15 sec  | No               | Off (01:00 on) |
| 04:00-13:00 | 30 min    | Single read   | No               | On (off at 13:00) |
| 13:00-22:00 | 30 min    | Single read   | Yes (GPS+radio off, CPU sleeps 28 min) | Off |
| 22:00-24:00 | 30 sec    | Every 15 sec  | No               | Off |

- Timeslot offset: 600ms (TXTENTHSSLOT=6)

## LoRa Message Format
```
|<msg_type>,<device_id>,<HHMMSS>,<lat>,<long>,<battery>
```
Example: `|2,6,183045,43.177073,-77.483594,3547`

## Key Notes
- GPS reads every 15 seconds, buffers up to 2 positions per transmit
- Nav light on GPIO6, active UTC 01:00-13:00, flashes 0.5s every 20s
- Display auto-off after 5 minutes from boot
- `transmitStr` buffer (200 bytes) is only cleared in `OnTxDone` — watch for overflow if transmits fail
- GitHub: https://github.com/SailorScott/LoRaBuoySend.git
