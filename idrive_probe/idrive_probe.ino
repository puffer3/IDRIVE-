/*
 * idrive_probe — measure the ACTUAL voltage between every pair of encoder
 * wires, instead of guessing from a logic threshold.
 *
 * Board: Teensy 3.2 / 3.1     Tools > USB Type: Serial
 *
 * WIRING  (note the encoder moved to analog-capable pins)
 * ------------------------------------------------------
 *   5 encoder wires (from the KNOB MODULE) -> pins 18, 19, 20, 21, 22
 *   4 direction-switch wires               -> pins 14, 15, 16, 17   (unchanged)
 *   switch common                          -> Teensy GND            (unchanged)
 *
 * WHY
 * ---
 * The previous scan saw pin 4 and pin 6 flickering at the logic threshold
 * thousands of times a second. A real switch contact reads a hard zero; a
 * reading that sits exactly ON the threshold means a semiconductor junction --
 * i.e. an LED. But digitalRead can only say "high or low", so it flaps.
 *
 * Pins 18-22 are A4-A8, so here we drive one wire LOW and ANALOG read the
 * others. The voltage tells you what is actually between them:
 *
 *     < 0.30 V   SHORT      a real contact / common connection
 *   0.50-2.20 V  JUNCTION   a diode -- the IR LED, or a transistor B-E
 *     > 2.80 V   open       nothing between them
 *
 * A junction that appears in ONE drive direction but not the reverse is a
 * diode, and its polarity tells you the anode: if driving X low shows a
 * junction on Y, then Y is the ANODE (current flows Y -> X) and X is the
 * cathode, which on a photointerrupter is usually module ground.
 *
 * Type 'm' for an immediate matrix. It also prints one every 3 s.
 * Switch presses print live the whole time.
 */

const uint8_t ENC[] = { 18, 19, 20, 21, 22 };
const uint8_t NENC  = sizeof(ENC) / sizeof(ENC[0]);

const uint8_t SW[]  = { 14, 15, 16, 17 };
const uint8_t NSW   = sizeof(SW) / sizeof(SW[0]);

bool     swState[NSW];
uint32_t swChangedAt[NSW];
uint32_t lastMatrix = 0;

const uint32_t DEBOUNCE_MS = 12;

float readVolts(uint8_t pin) {
  // average a few samples to kill noise
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

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) { }

  analogReadResolution(10);
  analogReadAveraging(8);

  for (uint8_t i = 0; i < NENC; i++) pinMode(ENC[i], INPUT_PULLUP);
  for (uint8_t i = 0; i < NSW; i++) {
    pinMode(SW[i], INPUT_PULLUP);
    swState[i] = false;
    swChangedAt[i] = 0;
  }

  Serial.println();
  Serial.println("=== iDrive analog probe ===");
  Serial.println("Encoder wires on 18,19,20,21,22   Switches on 14,15,16,17 + GND");
  Serial.println("Press each direction. Turn the knob during a matrix print.\n");
  lastMatrix = millis();
}

void matrix() {
  Serial.println("---- drive one wire LOW, measure the others ----");
  for (uint8_t i = 0; i < NENC; i++) {
    // release everything, then drive one low
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
      Serial.print(ENC[j]);
      Serial.print("=");
      Serial.print(v, 2);
      Serial.print("V ");
      Serial.print(classify(v));
      Serial.print("  ");
    }
    Serial.println();
  }
  for (uint8_t k = 0; k < NENC; k++) pinMode(ENC[k], INPUT_PULLUP);
  Serial.println();
}

void loop() {
  // --- switches, debounced, WITH the actual voltage ---
  // Pins 14-17 are A0-A3, so when one reads LOW we can ask what voltage it is
  // really sitting at. A hard 0.0-0.3 V means something is genuinely grounding
  // it. Anything drifting around 0.5-1.5 V means the pin is floating and being
  // dragged near the threshold by pickup, not by a switch.
  for (uint8_t i = 0; i < NSW; i++) {
    bool down = (digitalRead(SW[i]) == LOW);
    if (down != swState[i] && (millis() - swChangedAt[i]) > DEBOUNCE_MS) {
      swState[i] = down;
      swChangedAt[i] = millis();
      float v = readVolts(SW[i]);
      Serial.print(millis());
      Serial.print(down ? "  SWITCH pin " : "  release pin ");
      Serial.print(SW[i]);
      Serial.print("   measured ");
      Serial.print(v, 2);
      Serial.print("V  -> ");
      if (v < 0.30f)      Serial.println("HARD GROUNDED (real contact)");
      else if (v < 2.20f) Serial.println("FLOATING / pickup, NOT a real press");
      else                Serial.println("released");
    }
  }

  // Baseline: every 2 s print where all four switch pins are actually sitting.
  static uint32_t lastSw = 0;
  if (millis() - lastSw > 2000) {
    lastSw = millis();
    Serial.print("   switch rail:");
    for (uint8_t i = 0; i < NSW; i++) {
      Serial.print(' '); Serial.print(SW[i]); Serial.print('=');
      Serial.print(readVolts(SW[i]), 2); Serial.print('V');
    }
    Serial.println();
  }

  if (Serial.available() && Serial.read() == 'm') matrix();

  if (millis() - lastMatrix > 3000) { lastMatrix = millis(); matrix(); }
}
