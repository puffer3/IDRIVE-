/*
 * idrive_monitor — BMW iDrive knob as a monitor controller. Teensy 3.2/3.1.
 *
 * Tools > Board    : Teensy 3.2 / 3.1
 * Tools > USB Type : Serial + MIDI   (the faders send MIDI CC; Serial keeps
 *                                     the debug console)
 *
 * WIRING
 *   encoder V+   -> 3.3V        encoder GND -> GND
 *   encoder A    -> 20          encoder B   -> 21
 *   push         -> 19
 *   direction    -> 14, 15, 16, 17     (their common -> GND)
 *   fader 1      -> wiper 22, ends 3.3V / GND
 *   fader 2      -> wiper 23, ends 3.3V / GND
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
 *     onFader(uint8_t i, uint8_t value)      i = 0..1, value 0..127
 * ------------------------------------------------------------------
 */

#include <Encoder.h>

// ---- pins ----------------------------------------------------------------
const uint8_t PIN_A    = 20;
const uint8_t PIN_B    = 21;
const uint8_t PIN_PUSH = 19;
const uint8_t DIR_PIN[4] = { 14, 15, 16, 17 };
const uint8_t FADER_PIN[2] = { 22, 23 };   // A8, A9 -- wipers
const uint8_t FADER_VCC    = 18;           // driven HIGH = 3.3V supply for the fader hot ends

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

// ---- faders --------------------------------------------------------------
// Sample-bank select. One absolute CC per fader, 0..127.
// Apollo monitor-control map (matches ~/Downloads/config.json):
//   knob turn = CC16 relative; buttons 17 mute, 18 dim, 19 alt, 20 mono, 21 console
const uint8_t ROTATE_CC     = 16;          // knob turn, relative (65 up / 63 down)
const uint8_t PUSH_CC       = 17;          // knob press = mute
const uint8_t DIR_CC_MAP[4] = { 20, 18, 19, 21 };   // indexed like DIR_PIN: RIGHT=mono, UP=dim, LEFT=alt, DOWN=console
const uint8_t FADER_CC[2]   = { 1, 11 };   // fader 1 = Modulation, fader 2 = Expression
const uint8_t MIDI_CH       = 1;
// Set true for a fader that reads backwards (3.3V and GND ends swapped).
const bool    FADER_FLIP[2] = { false, false };
// Raw 10-bit counts the wiper must move before a new CC is sent. One MIDI
// step is 8 counts, so 5 kills jitter without losing any single step.
const int     FADER_DEADBAND = 8;
// Raw values at/below and at/above these clamp to 0 and 127, so the ends of
// travel land reliably on the end values even if the track doesn't quite
// reach the rails.
const int     FADER_RAW_LO  = 8;
const int     FADER_RAW_HI  = 1015;

// ==========================================================================
//  HOOKS — put your monitor-control logic here
// ==========================================================================

void onRotate(int8_t dir) {
  Serial.print("ROTATE ");
  Serial.println(dir > 0 ? "CW" : "CCW");

  // Relative CC: 65 = one click up, 63 = one click down.
  usbMIDI.sendControlChange(ROTATE_CC, dir > 0 ? 65 : 63, MIDI_CH);
}

void onPush(bool pressed) {
  Serial.println(pressed ? "PUSH  pressed" : "PUSH  released");

  usbMIDI.sendControlChange(PUSH_CC, pressed ? 127 : 0, MIDI_CH);
}

void onDirection(uint8_t i, bool pressed) {
  Serial.print(DIR_NAME[i]);
  Serial.println(pressed ? "  pressed" : "  released");

  usbMIDI.sendControlChange(DIR_CC_MAP[i], pressed ? 127 : 0, MIDI_CH);
}

void onFader(uint8_t i, uint8_t value) {
  Serial.print("FADER "); Serial.print(i + 1);
  Serial.print(" = ");    Serial.println(value);

  usbMIDI.sendControlChange(FADER_CC[i], value, MIDI_CH);
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

// ---- faders: raw 10-bit -> 7-bit with deadband ----
int     faderRaw[2]  = { -1000, -1000 };   // last raw value that produced a send
uint8_t faderVal[2]  = { 255, 255 };       // last 7-bit value sent

uint8_t faderTo7(int raw) {
  if (raw <= FADER_RAW_LO) return 0;
  if (raw >= FADER_RAW_HI) return 127;
  return (uint8_t)(((long)(raw - FADER_RAW_LO) * 127) / (FADER_RAW_HI - FADER_RAW_LO));
}

void updateFader(uint8_t i) {
  int raw = analogRead(FADER_PIN[i]);
  if (FADER_FLIP[i]) raw = 1023 - raw;
  if (abs(raw - faderRaw[i]) < FADER_DEADBAND) return;
  uint8_t v = faderTo7(raw);
  faderRaw[i] = raw;
  if (v == faderVal[i]) return;
  faderVal[i] = v;
  onFader(i, v);
}

void setup() {
  Serial.begin(115200);

  pinMode(FADER_VCC, OUTPUT);
  digitalWrite(FADER_VCC, HIGH);   // faders are fed from pin 18, not the 3.3V pin

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
  // Faders are driven by the pot itself -- no pull-up, or it skews the wiper.
  for (uint8_t i = 0; i < 2; i++) pinMode(FADER_PIN[i], INPUT);

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
  Serial.print(" faders=");
  for (uint8_t i = 0; i < 2; i++) { Serial.print(analogRead(FADER_PIN[i])); Serial.print(' '); }
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

  // --- faders ---
  for (uint8_t i = 0; i < 2; i++) updateFader(i);

  // Drain incoming MIDI so the host-side buffer never fills.
  while (usbMIDI.read()) {}

  // --- console helpers ---
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'z') { enc.write(0); lastPos = 0; Serial.println("count zeroed"); }
    if (c == 'r') showRaw();
  }
}
