/*
 * idrive_knob — BMW iDrive knob (rotary + push + 4-way) on a Teensy 3.1
 *
 * Board: Teensy 3.2 / 3.1     Tools > USB Type: Serial  (switch to MIDI or
 *                                                        Keyboard once it works)
 *
 * Run idrive_pinscan first to learn which wire is which, then fill in the pin
 * numbers below. Every input uses the Teensy's internal pullup, so each switch
 * and encoder contact just needs its other side tied to GND — no external
 * resistors.
 *
 * Encoder and Bounce2 both ship with Teensyduino; no extra downloads.
 */

#include <Encoder.h>
#include <Bounce2.h>

// ---- fill these in from the pin scan -------------------------------------
const uint8_t PIN_ENC_A = 2;
const uint8_t PIN_ENC_B = 3;
const uint8_t PIN_PUSH  = 4;

const uint8_t PIN_UP    = 7;
const uint8_t PIN_DOWN  = 8;
const uint8_t PIN_LEFT  = 9;
const uint8_t PIN_RIGHT = 10;

// Most detented encoders emit 4 quadrature counts per physical click. If one
// click of the knob reports 4, leave this at 4. If it reports 1, set it to 1;
// if 2, set it to 2.
const int8_t COUNTS_PER_DETENT = 4;
// --------------------------------------------------------------------------

Encoder enc(PIN_ENC_A, PIN_ENC_B);

struct Btn { uint8_t pin; const char *name; Bounce db; };
Btn buttons[] = {
  { PIN_PUSH,  "PUSH",  Bounce() },
  { PIN_UP,    "UP",    Bounce() },
  { PIN_DOWN,  "DOWN",  Bounce() },
  { PIN_LEFT,  "LEFT",  Bounce() },
  { PIN_RIGHT, "RIGHT", Bounce() },
};
const uint8_t NBTN = sizeof(buttons) / sizeof(buttons[0]);

long lastPos = 0;

void setup() {
  Serial.begin(115200);

  for (uint8_t i = 0; i < NBTN; i++) {
    pinMode(buttons[i].pin, INPUT_PULLUP);
    buttons[i].db.attach(buttons[i].pin);
    buttons[i].db.interval(8);            // ms; raise if a switch chatters
  }

  enc.write(0);
  Serial.println("iDrive knob ready.");
}

// Called once per detent. delta is -1 or +1.
void onRotate(int8_t delta) {
  Serial.print("ROTATE ");
  Serial.println(delta > 0 ? "CW" : "CCW");

  // usbMIDI.sendControlChange(16, delta > 0 ? 65 : 63, 1);   // relative CC
}

void onPress(const char *name) {
  Serial.print("PRESS   ");
  Serial.println(name);
}

void onRelease(const char *name) {
  Serial.print("RELEASE ");
  Serial.println(name);
}

void loop() {
  // --- rotary ---
  long pos = enc.read();
  long diff = pos - lastPos;
  while (diff >= COUNTS_PER_DETENT) { onRotate(+1); lastPos += COUNTS_PER_DETENT; diff -= COUNTS_PER_DETENT; }
  while (diff <= -COUNTS_PER_DETENT) { onRotate(-1); lastPos -= COUNTS_PER_DETENT; diff += COUNTS_PER_DETENT; }

  // --- buttons ---
  for (uint8_t i = 0; i < NBTN; i++) {
    buttons[i].db.update();
    if (buttons[i].db.fell()) onPress(buttons[i].name);      // pullup: pressed = LOW
    if (buttons[i].db.rose()) onRelease(buttons[i].name);
  }
}
