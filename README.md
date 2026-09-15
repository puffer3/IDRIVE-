# BMW iDrive knob → Teensy 3.1 — handoff

Sketch: `idrive_monitor.ino`. Board: **Teensy 3.2 / 3.1**. USB Type: **Serial + MIDI** (faders send MIDI CC; Serial keeps the debug console).

## Wiring

| Knob wire | Teensy |
|---|---|
| encoder V+ | **3.3V** |
| encoder GND | **GND** |
| encoder A | **20** |
| encoder B | **21** |
| push | **19** |
| direction — right | **14** |
| direction — up | **15** |
| direction — left | **16** |
| direction — down | **17** |
| direction common | **GND** |
| fader 1 wiper | **22** (A8) |
| fader 2 wiper | **23** (A9) |
| fader hot ends (both) | **18** — driven HIGH in `setup()` as a 3.3 V supply, because the real 3.3V pin is taken by the knob |
| fader ground ends (both) | **AGND** |

Runs at 3.3 V. No external resistors needed.

All measured and verified on the bench: rotation both ways, push, and all four directions.

**The push is inverted relative to the direction switches.** Pin 19 idles low and goes high when pressed; the four directions idle high and go low. `BTN_INVERT[]` handles this — index 0 is the push. If you rewire and the push starts reporting backwards, that flag is why.

## How to use the three inputs

Everything happens in three hook functions near the top of the sketch. Edit those; ignore the rest.

### Knob rotation

```cpp
void onRotate(int8_t dir) {
  // dir is -1 (CCW) or +1 (CW). Called once per detent.
}
```

For a volume control, send a **relative** CC so it never jumps:

```cpp
usbMIDI.sendControlChange(7, dir > 0 ? 65 : 63, 1);
```

63 = down one, 65 = up one, in the usual relative-CC convention. If your receiving software expects absolute values, keep a local `int volume` and clamp it 0–127 instead.

### Push

```cpp
void onPush(bool pressed) {
  // called on both press and release
}
```

Guard with `if (pressed)` for a mute toggle, so it fires once rather than twice.

### Four directions

```cpp
void onDirection(uint8_t i, bool pressed) {
  // i is 0..3, matching DIR_PIN / DIR_NAME
}
```

`i` indexes `DIR_PIN[] = {14, 15, 16, 17}`, which is `DIR_NAME[] = {RIGHT, UP, LEFT, DOWN}`.

### Faders

```cpp
void onFader(uint8_t i, uint8_t value) {
  // i is 0..1, value 0..127. Called only when the 7-bit value changes.
}
```

Sends absolute CC `FADER_CC[] = {1, 11}` (fader 1 = Modulation, fader 2 = Expression) on `MIDI_CH` 1 for sample-bank select. Change those constants to whatever the receiving software MIDI-learns. `FADER_DEADBAND` (raw counts, now **8** — 5 let the parked faders chatter) kills wiper jitter; `FADER_RAW_LO/HI` clamp the ends so 0 and 127 are always reachable.

## Calibrating the detent size

`COUNTS_PER_DETENT` is set to **2**, measured on this knob: one detent moves the count by exactly 2.

Already correct — you shouldn't need to touch it. To re-measure after a rewire: type `z` to zero the count, turn exactly one detent, type `r` and read `count=`. Symptom of a wrong value: one click gives several volume steps, or several clicks give one.

## Why the buttons use analogRead

The five switch lines idle around **2.0 V**, which sits right on the 3.3 V logic threshold (~1.98 V). `digitalRead` on a pin parked there returns whatever it feels like — it produced phantom presses lasting seconds during bring-up. Pressed is a clean 0 V, so comparing `analogRead` against `PRESS_THRESHOLD` (310 ≈ 1.0 V) is completely stable.

**Don't convert these back to `digitalRead`.** It'll appear to work and then misfire.

The encoder A/B lines are different — those are driven properly by the sensor IC, so they're plain digital and the `Encoder` library handles them.

## Building from the command line

```
arduino-cli compile -b teensy:avr:teensy31:usb=serialmidi "$HOME/GitHub/idrive-knob/IDRIVE /idrive_monitor"
```

Upload without needing a port (the Teensy loader reboots the board itself):

```
T=~/Library/Arduino15/packages/teensy/tools/teensy-tools/1.62.0
$T/teensy_post_compile -file=idrive_monitor.ino -path=<build-dir> -tools=$T -board=TEENSY31 -reboot
```

`arduino-cli upload` fails with "no upload port provided" when the board is in the bootloader — use the above instead.

## If it stops working

- **No serial port, LED blinking** — the board is sitting in the HalfKay bootloader. The `Program` pin on the 5-pin end header is next to `GND`; a bridge or stray wire between them holds it there forever.
- **Board vanishes from USB** — check the encoder's V+ and GND aren't touching. That rail is fed from the Teensy, and shorting it drops the USB connection.
- **A direction stops responding** — it was a solder joint last time, not the pin.

## Other sketches in this repo

Diagnostics kept from bring-up, in case you add more wires later:

- `idrive_encmatrix` — drives each wire low and measures the others. Finds V+/GND/signals on an unknown connector by reading the diode drops.
- `idrive_switches` — alternates passive voltage reads and a continuity matrix. Use it to identify unknown switch wires.
- `idrive_watch` — passive listen on three lines, counts edges. Good for "is this thing doing anything at all".

## Still open

- The knob bundle has 8 wires; 5 are in use (4 directions + common). The other 3 are unidentified — one of them is an LED that got wired in by mistake during bring-up.
- `arduino-cli monitor` doesn't work here ("No monitor available for the port protocol serial"). Use `screen /dev/cu.usbmodem5324901 115200` instead; quit with Ctrl-A K y.
- Note `screen` holds the port, so quit it before flashing or the upload fails with "Unable to open ... for reboot request".
