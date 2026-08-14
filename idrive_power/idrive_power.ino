/*
 * idrive_power — power the encoder module and identify its outputs.
 *
 * Board: Teensy 3.2 / 3.1     Tools > USB Type: Serial
 *
 * WIRING CHANGE — ONE RESISTOR NEEDED
 * -----------------------------------
 *   Teensy pin 18 --[ 220 ohm ]-- encoder common wire   <-- the wire that was
 *                                                            on pin 18
 *   remaining 4 encoder wires -> pins 19, 20, 21, 22    (unchanged)
 *
 * Anything from 150 to 470 ohms is fine. At ~1.7 V across the junction that
 * gives roughly 7 mA, which is a sane IR emitter current and well inside what
 * a Teensy pin can source.
 *
 * WHAT IT DOES
 * ------------
 * We established that pin 18 is the module's V+ and that 19-22 are all on the
 * low side of it. We do NOT yet know which of those four is module ground and
 * which are signal outputs. So this sketch searches:
 *
 *   for each candidate ground G in {19,20,21,22}:
 *       drive G low (a real ground), leave the other three pulled up
 *       measure all three with the emitter ON, then again with it OFF
 *       report the difference
 *
 * A phototransistor output changes when the light behind it changes. So any
 * pin whose voltage MOVES between emitter-on and emitter-off is a real optical
 * output -- and that identifies both the correct ground and the signal pins
 * without you having to touch the knob at all.
 *
 * Then, once a promising configuration is found, turn the knob slowly: the
 * outputs should swing as the encoder wheel chops the beam.
 *
 * Commands:  's' = re-run the search      'l' = live mode on last good config
 */

const uint8_t LED_PIN = 18;               // via the series resistor
const uint8_t CAND[]  = { 19, 20, 21, 22 };
const uint8_t NC      = sizeof(CAND) / sizeof(CAND[0]);

int8_t   bestGround = -1;
uint32_t lastRun = 0;
bool     liveMode = false;

float readVolts(uint8_t pin) {
  uint32_t acc = 0;
  for (uint8_t k = 0; k < 8; k++) acc += analogRead(pin);
  return (acc / 8.0f) * 3.3f / 1023.0f;
}

void emitter(bool on) {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, on ? HIGH : LOW);
  delay(3);
}

void search() {
  Serial.println();
  Serial.println("======== searching for module ground + outputs ========");
  float bestDelta = 0;
  bestGround = -1;

  for (uint8_t g = 0; g < NC; g++) {
    // candidate ground driven low; the rest pulled up as inputs
    for (uint8_t k = 0; k < NC; k++) pinMode(CAND[k], INPUT_PULLUP);
    pinMode(CAND[g], OUTPUT);
    digitalWrite(CAND[g], LOW);
    delay(5);

    Serial.print("  ground = ");
    Serial.print(CAND[g]);
    Serial.print(" : ");

    float maxDelta = 0;
    for (uint8_t j = 0; j < NC; j++) {
      if (j == g) continue;
      emitter(true);  float vOn  = readVolts(CAND[j]);
      emitter(false); float vOff = readVolts(CAND[j]);
      float d = fabs(vOn - vOff);
      if (d > maxDelta) maxDelta = d;

      Serial.print(CAND[j]); Serial.print(": on=");
      Serial.print(vOn, 2);  Serial.print("V off=");
      Serial.print(vOff, 2); Serial.print("V  d=");
      Serial.print(d, 2);
      if (d > 0.20f) Serial.print(" <== RESPONDS TO LIGHT");
      Serial.print("   ");
    }
    Serial.println();

    if (maxDelta > bestDelta) { bestDelta = maxDelta; bestGround = g; }
  }

  Serial.println("-------------------------------------------------------");
  if (bestDelta > 0.20f) {
    Serial.print("  BEST: ground on pin ");
    Serial.print(CAND[bestGround]);
    Serial.print("  (largest light response ");
    Serial.print(bestDelta, 2);
    Serial.println(" V)");
    Serial.println("  Entering live mode - TURN THE KNOB SLOWLY now.");
    liveMode = true;
  } else {
    Serial.println("  No pin responded to the emitter.");
    Serial.println("  Check the 220 ohm resistor is between pin 18 and the");
    Serial.println("  common wire, and that pin 18 really is the V+ wire.");
    liveMode = false;
  }
  Serial.println();
}

void live() {
  // hold the best configuration and stream the other pins
  for (uint8_t k = 0; k < NC; k++) pinMode(CAND[k], INPUT_PULLUP);
  pinMode(CAND[bestGround], OUTPUT);
  digitalWrite(CAND[bestGround], LOW);
  emitter(true);

  Serial.print(millis());
  Serial.print("  ");
  for (uint8_t j = 0; j < NC; j++) {
    if (j == (uint8_t)bestGround) continue;
    Serial.print(CAND[j]); Serial.print("=");
    Serial.print(readVolts(CAND[j]), 2); Serial.print("V  ");
  }
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) { }
  analogReadResolution(10);
  analogReadAveraging(8);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  for (uint8_t k = 0; k < NC; k++) pinMode(CAND[k], INPUT_PULLUP);

  Serial.println();
  Serial.println("=== iDrive powered-encoder probe ===");
  Serial.println("Emitter drive on pin 18 (through your series resistor).");
  Serial.println("Candidates: 19 20 21 22");
  delay(500);
  search();
  lastRun = millis();
}

void loop() {
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 's') search();
    if (c == 'l') liveMode = !liveMode;
  }

  if (liveMode && bestGround >= 0) {
    live();
    delay(120);
  } else if (millis() - lastRun > 8000) {
    lastRun = millis();
    search();
  }
}
