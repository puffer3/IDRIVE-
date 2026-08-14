/*
 * idrive_monitor — BMW iDrive knob as a monitor controller. Teensy 3.2/3.1.
 *
 * Tools > Board    : Teensy 3.2 / 3.1
 * Tools > USB Type : Serial   (change to MIDI or Serial+MIDI when you wire it
 *                              to whatever you're controlling)
 *
 * WIRING
 *   encoder V+   -> 3.3V        encoder GND -> GND
 *   encoder A    -> 20          encoder B   -> 21
 *   push         -> 19
 *   direction    -> 14, 15, 16, 17     (their common -> GND)
 *
 * The direction switches and the push are read with analogRead against a
 * threshold, not digitalRead. They idle near 2.0 V, which is right on the
 * 3.3 V logic boundary, so digitalRead flickers on them. Pressed is a clean
 * 0 V, so an analog threshold is completely reliable. Do not "simplify" these
 * back to digitalRead.
 *
 * ------------------------------------------------------------------
 * THE ONLY THREE THINGS YOU NEED TO TOUCH ARE THE HOOKS NEAR THE TOP:
 *     onRotate(int8_t dir)          dir is -1 or +1, one call per detent
 *     onPush(bool pressed)
 *     onDirection(uint8_t i, bool pressed)   i = 0..3, see DIR_NAME
 * ------------------------------------------------------------------
 */

#include <Encoder.h>

// ---- pins ----------------------------------------------------------------
const uint8_t PIN_A    = 20;
const uint8_t PIN_B    = 21;
const uint8_t PIN_PUSH = 19;
const uint8_t DIR_PIN[4] = { 14, 15, 16, 17 };

// Measured on the knob: 14=right, 15=up, 16=left, 17=down.
const char *DIR_NAME[4] = { "RIGHT", "UP", "LEFT", "DOWN" };

// The push on pin 19 idles LOW and goes HIGH when pressed -- the opposite of
// the four direction switches. Index 0 is the push.
const bool BTN_INVERT[5] = { true, false, false, false, false };

// ---- tuning --------------------------------------------------------------
// Quadrature counts per physical click. Measured on this knob: one detent
// moves the count by exactly 2 (AB goes 00 -> 01 -> 11 and stops there).
const int8_t COUNTS_PER_DETENT = 2;

// Pressed if the pin reads below this. ~1.0 V on a 3.3 V / 10-bit ADC.
const int    PRESS_THRESHOLD   = 310;

const uint32_t DEBOUNCE_MS     = 15;

// ==========================================================================
//  HOOKS — put your monitor-control logic here
// ==========================================================================

void onRotate(int8_t dir) {
  Serial.print("ROTATE ");
  Serial.println(dir > 0 ? "CW" : "CCW");

  // Volume as a relative MIDI CC (needs USB Type = MIDI or Serial+MIDI):
  // usbMIDI.sendControlChange(7, dir > 0 ? 65 : 63, 1);
}

void onPush(bool pressed) {
  Serial.println(pressed ? "PUSH  pressed" : "PUSH  released");

  // Mute toggle on press:
  // if (pressed) usbMIDI.sendControlChange(20, 127, 1);
}

void onDirection(uint8_t i, bool pressed) {
  Serial.print(DIR_NAME[i]);
  Serial.println(pressed ? "  pressed" : "  released");

  // Source select, one CC per direction:
  // if (pressed) usbMIDI.sendControlChange(30 + i, 127, 1);
}

// ==========================================================================
//  Everything below is plumbing
// ==========================================================================

Encoder enc(PIN_A, PIN_B);
long lastPos = 0;

// Index 0 is the push; 1..4 are the directions, matching DIR_PIN.
// Plain arrays rather than a struct: the .ino preprocessor hoists function
// prototypes above any struct you declare here, which won't compile.
uint8_t  btnPin[5];
bool     btnState[5];
uint32_t btnChanged[5];

// -1 = no change, 0 = just released, 1 = just pressed
int8_t updateBtn(uint8_t i) {
  bool now = (analogRead(btnPin[i]) < PRESS_THRESHOLD);
  if (BTN_INVERT[i]) now = !now;
  if (now != btnState[i] && (millis() - btnChanged[i]) > DEBOUNCE_MS) {
    btnState[i]   = now;
    btnChanged[i] = millis();
    return now ? 1 : 0;
  }
  return -1;
}

void setup() {
  Serial.begin(115200);

  // Do NOT call pinMode on PIN_A / PIN_B. The Encoder object is constructed
  // before setup() runs and configures those pins itself; overriding them
  // here stops it counting.

  btnPin[0] = PIN_PUSH;
  for (uint8_t i = 0; i < 4; i++) btnPin[i + 1] = DIR_PIN[i];
  for (uint8_t i = 0; i < 5; i++) {
    btnState[i]   = false;
    btnChanged[i] = 0;
    pinMode(btnPin[i], INPUT_PULLUP);
  }

  analogReadResolution(10);
  analogReadAveraging(8);
  enc.write(0);

  Serial.println("\niDrive monitor controller ready.");
  Serial.println("  z = zero the count (turn one full revolution to measure PPR)");
  Serial.println("  r = show raw values\n");
}

void showRaw() {
  Serial.print("raw  A=");  Serial.print(digitalRead(PIN_A));
  Serial.print(" B=");      Serial.print(digitalRead(PIN_B));
  Serial.print("  count="); Serial.print(enc.read());
  Serial.print("  push=");  Serial.print(analogRead(PIN_PUSH));
  Serial.print("  dirs=");
  for (uint8_t i = 0; i < 4; i++) { Serial.print(analogRead(DIR_PIN[i])); Serial.print(' '); }
  Serial.println();
}

void loop() {
  // --- rotation, converted to whole detents ---
  long pos  = enc.read();
  long diff = pos - lastPos;
  while (diff >=  COUNTS_PER_DETENT) { onRotate(+1); lastPos += COUNTS_PER_DETENT; diff -= COUNTS_PER_DETENT; }
  while (diff <= -COUNTS_PER_DETENT) { onRotate(-1); lastPos -= COUNTS_PER_DETENT; diff += COUNTS_PER_DETENT; }

  // --- buttons ---
  int8_t ev = updateBtn(0);
  if (ev >= 0) onPush(ev == 1);
  for (uint8_t i = 0; i < 4; i++) {
    ev = updateBtn(i + 1);
    if (ev >= 0) onDirection(i, ev == 1);
  }

  // --- console helpers ---
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'z') { enc.write(0); lastPos = 0; Serial.println("count zeroed"); }
    if (c == 'r') showRaw();
  }
}
