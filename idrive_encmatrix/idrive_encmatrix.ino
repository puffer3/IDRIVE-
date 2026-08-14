/*
 * idrive_encmatrix — voltage map of the FIVE ENCODER wires.
 *
 * Board: Teensy 3.2 / 3.1     Tools > USB Type: Serial
 *
 * WIRING — this is the important part
 * -----------------------------------
 *   The 5 ENCODER wires -> pins 14, 19, 20, 21, 22      <-- MOVE THESE
 *   The 4 switch wires  -> pins 15, 16, 17, 18          <-- LEAVE ALONE
 *   switch common       -> Teensy GND                   <-- LEAVE ALONE
 *
 * Why move the encoder: only pins 14-23 on a Teensy 3.1 can do analogRead.
 * Pins 2-7 cannot, which is why the encoder only ever gave us ambiguous
 * "flapping at the logic threshold" instead of real numbers. Same wires, same
 * knob -- they just have to live somewhere measurable. 14 and 19-22 are the
 * analog pins the switches aren't already using, so nothing else has to move.
 *
 * WHAT IT PRINTS
 * --------------
 * Drives each wire low in turn and measures the other four:
 *
 *     < 0.30 V   SHORT      a real contact or a direct common connection
 *   0.50-2.20 V  JUNCTION   a diode -- an emitter, or a transistor junction
 *     > 2.80 V   open       nothing between them
 *
 * Read the result like this:
 *   - A pin that shows a junction to every other pin, in one direction only,
 *     is the module's common rail.
 *   - A junction appearing in ONE direction but not the reverse is a diode;
 *     current flows from the pin reading the junction toward the driven pin,
 *     so the pin showing the junction is the ANODE.
 *   - Any pair that goes SHORT and then open as you rotate would mean
 *     mechanical contacts after all.
 *
 * It also watches for CHANGE while you turn: if any reading moves by more
 * than 0.15 V between sweeps it flags it, since that is the signature of a
 * real encoder output rather than a static junction.
 *
 * Type 'm' to force an immediate sweep.
 */

// Deliberately avoiding pin 14. Measurement says it is healthy (3.08 V with
// nothing attached, and it behaves like every other pin), and the phantom
// presses came from a wire sitting at 1.16 V in the indeterminate band rather
// than from the pin itself -- but 23 is also analog (A9) and free, so there is
// no reason to leave any doubt in the setup.
const uint8_t ENC[] = { 19, 20, 21, 22, 23 };
const uint8_t NENC  = sizeof(ENC) / sizeof(ENC[0]);

// Switches stay on 15-18 with their common on GND. We only watch these; we
// never drive them. A real switch reads ~3.1 V open and ~0.0 V pressed.
// Anything parked mid-scale is NOT a switch.
const uint8_t SWP[] = { 15, 16, 17, 18 };
const uint8_t NSWP  = sizeof(SWP) / sizeof(SWP[0]);

float prev[NENC][NENC];
bool  havePrev = false;
uint32_t lastSweep = 0;

float readVolts(uint8_t pin) {
  uint32_t acc = 0;
  for (uint8_t k = 0; k < 8; k++) acc += analogRead(pin);
  return (acc / 8.0f) * 3.3f / 1023.0f;
}

const char *classify(float v) {
  if (v < 0.30f) return "SHORT   ";
  if (v < 2.20f) return "JUNCTION";
  if (v < 2.80f) return "leaky   ";
  return "open    ";
}

void sweep() {
  Serial.println("---- drive one encoder wire LOW, measure the others ----");
  bool moved = false;

  for (uint8_t i = 0; i < NENC; i++) {
    for (uint8_t k = 0; k < NENC; k++) pinMode(ENC[k], INPUT_PULLUP);
    pinMode(ENC[i], OUTPUT);
    digitalWrite(ENC[i], LOW);
    delay(2);

    Serial.print("  drive ");
    Serial.print(ENC[i]);
    Serial.print(" LOW : ");
    for (uint8_t j = 0; j < NENC; j++) {
      if (j == i) continue;
      float v = readVolts(ENC[j]);
      Serial.print(ENC[j]); Serial.print("=");
      Serial.print(v, 2);   Serial.print("V ");
      Serial.print(classify(v));
      if (havePrev && fabs(v - prev[i][j]) > 0.15f) { Serial.print("*CHANGED*"); moved = true; }
      Serial.print("  ");
      prev[i][j] = v;
    }
    Serial.println();
  }

  for (uint8_t k = 0; k < NENC; k++) pinMode(ENC[k], INPUT_PULLUP);
  havePrev = true;

  // Report the switch pins too, as raw volts, so we can settle what they are.
  Serial.print("  switches 15-18 :");
  for (uint8_t k = 0; k < NSWP; k++) {
    float v = readVolts(SWP[k]);
    Serial.print(' '); Serial.print(SWP[k]); Serial.print('=');
    Serial.print(v, 2); Serial.print('V');
    if (v < 0.30f)      Serial.print("(PRESSED)");
    else if (v < 2.20f) Serial.print("(mid-scale: NOT a switch)");
  }
  Serial.println();

  if (moved) Serial.println("  >>> something MOVED between sweeps - keep turning!");
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) { }
  analogReadResolution(10);
  analogReadAveraging(8);
  for (uint8_t k = 0; k < NENC; k++) pinMode(ENC[k], INPUT_PULLUP);
  for (uint8_t k = 0; k < NSWP; k++) pinMode(SWP[k], INPUT_PULLUP);

  Serial.println();
  Serial.println("=== iDrive ENCODER voltage matrix ===");
  Serial.println("Encoder wires expected on 14, 19, 20, 21, 22");
  Serial.println("Switches watched on 15, 16, 17, 18 (common to GND)");
  Serial.println("Turn the knob slowly and steadily.\n");
  lastSweep = millis();
}

void loop() {
  if (Serial.available() && Serial.read() == 'm') sweep();
  if (millis() - lastSweep > 1500) { lastSweep = millis(); sweep(); }
}
