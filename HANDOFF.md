# iDrive knob + faders — handoff (2026-09-15)

Teensy 3.1 · sketch `idrive_monitor/idrive_monitor.ino` · board `Teensy 3.2/3.1` · USB Type **Serial + MIDI**
Repo: github.com/puffer3/IDRIVE- (`main` is current, commit 803a6dc = what is flashed on the board)

## Wiring

Hold the Teensy with the USB at the top, chip side up. Two long rows of pins.

**Right row, counting DOWN from the USB end:**

| # | pin  | connect to |
|---|------|------------|
| 1 | Vin  | nothing |
| 2 | AGND | both faders' GROUND ends |
| 3 | 3.3V | knob supply |
| 4 | 23   | fader 2 wiper (middle pin) |
| 5 | 22   | fader 1 wiper (middle pin) |
| 6 | 21   | encoder B |
| 7 | 20   | encoder A |
| 8 | 19   | knob push switch |
| 9 | 18   | both faders' HOT ends (sketch drives this pin HIGH = 3.3 V) |
| 10 | 17  | direction DOWN |
| 11 | 16  | direction LEFT |
| 12 | 15  | direction UP |
| 13 | 14  | direction RIGHT |
| 14 | 13  | nothing (onboard LED) |

**Left row:** top pin is GND — knob ground + direction switches' common. Pins 0–12 below it are unused.

Faders: each has 3 pins. Outer two = ends (one to 18, one to AGND). Middle = wiper (22 or 23).
If a fader reads backwards, swap its two end wires, or set `FADER_FLIP[]` in the sketch.

## MIDI (channel 1, port "iDRIVE Port 1")

Matches `~/Downloads/config.json` (Apollo monitor-control app):

| control | CC | values |
|---------|----|--------|
| knob turn | 16 | relative: 65 = one click up, 63 = one click down |
| knob push | 17 | mute — 127 on press, 0 on release |
| direction UP | 18 | dim |
| direction LEFT | 19 | alt |
| direction RIGHT | 20 | mono |
| direction DOWN | 21 | console |
| fader 1 | 1 | absolute 0–127 |
| fader 2 | 11 | absolute 0–127 |

**Which direction = which button (18–21) is a GUESS.** The original firmware that had this map was overwritten without a backup and exists nowhere on this laptop. If the other computer has the original sketch or an Arduino build cache from mid-August, use that. Otherwise press each direction, watch a MIDI monitor, and fix the order in `DIR_CC_MAP[4] = { RIGHT, UP, LEFT, DOWN }` at the top of the sketch.

## Build / flash

```
cd "IDRIVE /idrive_monitor"
arduino-cli compile -b teensy:avr:teensy31:usb=serialmidi .
arduino-cli upload -p /dev/cu.usbmodem* -b teensy:avr:teensy31:usb=serialmidi .
```
Needs `arduino-cli` + `teensy:avr` core (1.62.0), or the Arduino IDE with Teensyduino.

**Before ANY future flash:** open a MIDI monitor, work every control, and save the log. Teensy firmware cannot be read back.

## Tunables (top of sketch)

`FADER_DEADBAND = 8` (was 5 — faders chattered at rest) · `FADER_RAW_LO/HI = 8/1015` · `COUNTS_PER_DETENT = 2` · `MIDI_CH = 1`

## Verified on the bench today

Both faders sweep 0–127 one step at a time, no idle jitter. Knob turn/push/all four directions arrive on the Mac's MIDI input. Knob's push line reads pressed only when pressed.
