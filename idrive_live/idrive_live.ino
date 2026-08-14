/*
 * idrive_live — the iDrive encoder, powered and running.
 *
 * Board: Teensy 3.2 / 3.1     Tools > USB Type: Serial
 *
 * PINOUT (derived from the ESD-diode topology, see below)
 * ------------------------------------------------------
 *   encoder V+   -> Teensy 3.3V pin      (was on pin 22)
 *   encoder GND  -> Teensy GND pin       (was on pin 21)
 *   output A     -> pin 19               (leave)
 *   output B     -> pin 20               (leave)
 *   push switch  -> pin 23               (leave)
 *
 * How we knew: driving pin 22 low put 19 at 0.56 V, but driving 19 low put 22
 * at 1.44 V -- one diode drop one way, blocked the other. Every diode in the
 * module pointed out of 21 and into 22, which is exactly the protection
 * structure around an IC's ground and supply rails. Pin 23 was isolated from
 * everything, which is what a bare mechanical push contact looks like.
 *
 * Start at 3.3 V. The donor board ran 5 V logic, so if the outputs never move
 * try 5 V on V+ instead (Teensy digital pins are 5 V tolerant, so reading them
 * is still safe) -- but try 3.3 V first, because it cannot damage a 5 V part
 * while the reverse is not true.
 *
 * WHAT IT PRINTS
 *   - every A/B transition, with the decoded direction
 *   - a running detent count
 *   - push press/release
 *
 * Turn the knob one full revolution and the PPR summary tells you how many
 * counts per turn, which is what idrive_knob.ino needs.
 */

#include <Encoder.h>
#include <Bounce2.h>

const uint8_t PIN_A    = 19;
const uint8_t PIN_B    = 20;
const uint8_t PIN_PUSH = 23;

Encoder enc(PIN_A, PIN_B);
Bounce  push = Bounce();

long     lastPos   = 0;
long     minPos    = 0, maxPos = 0;
uint32_t lastPrint = 0;
uint8_t  lastRaw   = 0xFF;

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) { }

  // The IC drives its outputs, but pullups are harmless and help if they turn
  // out to be open-collector.
  pinMode(PIN_A, INPUT_PULLUP);
  pinMode(PIN_B, INPUT_PULLUP);
  pinMode(PIN_PUSH, INPUT_PULLUP);
  push.attach(PIN_PUSH);
  push.interval(10);

  enc.write(0);

  Serial.println();
  Serial.println("=== iDrive encoder LIVE ===");
  Serial.println("A=19  B=20  push=23   (V+ -> 3.3V, GND -> GND)");
  Serial.println("Turn the knob a full revolution, then push it.\n");
  lastPrint = millis();
}

void loop() {
  // raw A/B state, so we can see the quadrature pattern even if the decode
  // is confused by wiring order
  uint8_t raw = (digitalRead(PIN_A) << 1) | digitalRead(PIN_B);
  if (raw != lastRaw) {
    lastRaw = raw;
    Serial.print(millis());
    Serial.print("  A=");
    Serial.print((raw >> 1) & 1);
    Serial.print(" B=");
    Serial.print(raw & 1);

    long p = enc.read();
    Serial.print("   count=");
    Serial.print(p);
    if (p > lastPos)      Serial.print("  CW");
    else if (p < lastPos) Serial.print("  CCW");
    lastPos = p;
    if (p < minPos) minPos = p;
    if (p > maxPos) maxPos = p;
    Serial.println();
  }

  push.update();
  if (push.fell()) Serial.println("           PUSH pressed");
  if (push.rose()) Serial.println("           PUSH released");

  if (millis() - lastPrint > 5000) {
    lastPrint = millis();
    Serial.print("   [range so far: ");
    Serial.print(minPos);
    Serial.print(" .. ");
    Serial.print(maxPos);
    Serial.print("  = ");
    Serial.print(maxPos - minPos);
    Serial.println(" counts of travel]");
  }
}
