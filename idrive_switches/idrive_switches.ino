/*
 * idrive_switches — work out the 4-way switch wiring, without disturbing the
 * encoder we just got working.
 *
 * Board: Teensy 3.2 / 3.1     Tools > USB Type: Serial
 *
 * WIRING
 *   encoder V+   -> Teensy 3.3V        (leave)
 *   encoder GND  -> Teensy GND         (leave)
 *   encoder A    -> pin 20             (leave)
 *   encoder B    -> pin 21             (leave)
 *   encoder push -> pin 19             (leave)
 *   4 switch wires -> pins 14, 15, 16, 17
 *
 * The encoder lines are only ever READ here, never driven -- the module is
 * powered now and its outputs are active.
 *
 * The switch pins get two different tests, alternating every 2 seconds:
 *
 *   PASSIVE — all four pulled up, just measure. If a wire's other end reaches
 *     ground through a closed switch you get ~0 V. If all four sit at the same
 *     mid-scale voltage they are commoned to each other and NOT independent
 *     switch legs, which is what we saw before.
 *
 *   MATRIX — drive one low, read the other three. This finds connections
 *     BETWEEN the wires, which is how we tell "4 switches sharing a common"
 *     from "4 wires that are all the same node".
 *
 * Press and hold each direction in turn while it runs.
 */

const uint8_t SW[] = { 14, 15, 16, 17 };
const uint8_t NSW  = sizeof(SW) / sizeof(SW[0]);

const uint8_t ENC[] = { 19, 20, 21 };     // read-only
const uint8_t NE    = sizeof(ENC) / sizeof(ENC[0]);

uint32_t encEdges[NE];
uint8_t  encLast[NE];
uint32_t lastTest = 0;
bool     doMatrix = false;

float readVolts(uint8_t pin) {
  uint32_t acc = 0;
  for (uint8_t k = 0; k < 8; k++) acc += analogRead(pin);
  return (acc / 8.0f) * 3.3f / 1023.0f;
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) { }
  analogReadResolution(10);
  analogReadAveraging(8);

  for (uint8_t i = 0; i < NSW; i++) pinMode(SW[i], INPUT_PULLUP);
  for (uint8_t i = 0; i < NE;  i++) { pinMode(ENC[i], INPUT); encEdges[i] = 0; }
  delay(20);
  for (uint8_t i = 0; i < NE; i++) encLast[i] = digitalRead(ENC[i]);

  Serial.println();
  Serial.println("=== iDrive switch hunt ===");
  Serial.println("Switches on 14,15,16,17.  Encoder read-only on 19,20,21.");
  Serial.println("Press and HOLD each direction for a second or two.\n");
  lastTest = millis();
}

void passive() {
  for (uint8_t i = 0; i < NSW; i++) pinMode(SW[i], INPUT_PULLUP);
  delay(2);
  Serial.print("PASSIVE :");
  bool allSame = true;
  float first = readVolts(SW[0]);
  for (uint8_t i = 0; i < NSW; i++) {
    float v = readVolts(SW[i]);
    if (fabs(v - first) > 0.10f) allSame = false;
    Serial.print(' '); Serial.print(SW[i]); Serial.print('=');
    Serial.print(v, 2); Serial.print('V');
    if (v < 0.30f)      Serial.print("(PRESSED)");
    else if (v > 2.80f) Serial.print("(open)");
    else                Serial.print("(mid)");
  }
  if (allSame) Serial.print("   <-- all equal: these wires are COMMONED");
  Serial.println();
}

void matrix() {
  Serial.println("MATRIX  :");
  for (uint8_t i = 0; i < NSW; i++) {
    for (uint8_t k = 0; k < NSW; k++) pinMode(SW[k], INPUT_PULLUP);
    pinMode(SW[i], OUTPUT);
    digitalWrite(SW[i], LOW);
    delay(2);
    Serial.print("   drive ");
    Serial.print(SW[i]);
    Serial.print(" LOW :");
    for (uint8_t j = 0; j < NSW; j++) {
      if (j == i) continue;
      float v = readVolts(SW[j]);
      Serial.print(' '); Serial.print(SW[j]); Serial.print('=');
      Serial.print(v, 2); Serial.print('V');
      if (v < 0.30f) Serial.print("(CONNECTED)");
    }
    Serial.println();
  }
  for (uint8_t k = 0; k < NSW; k++) pinMode(SW[k], INPUT_PULLUP);
}

void loop() {
  // keep an eye on the encoder so we know it still works
  for (uint8_t i = 0; i < NE; i++) {
    uint8_t now = digitalRead(ENC[i]);
    if (now != encLast[i]) { encLast[i] = now; encEdges[i]++; }
  }

  if (millis() - lastTest > 2000) {
    lastTest = millis();
    if (doMatrix) matrix(); else passive();
    doMatrix = !doMatrix;
    Serial.print("   (encoder edges 19/20/21: ");
    for (uint8_t i = 0; i < NE; i++) { Serial.print(encEdges[i]); Serial.print(' '); }
    Serial.println(")");
  }
}
