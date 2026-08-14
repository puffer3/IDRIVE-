/*
 * idrive_combo — encoder continuity matrix + direction switches, together.
 *
 * Board: Teensy 3.2 / 3.1     Tools > USB Type: Serial
 *
 * WIRING
 * ------
 *   5 encoder wires (from the KNOB MODULE, not the ALPS board) -> pins 2,3,4,5,6
 *   4 direction-switch wires                                   -> pins 14,15,16,17
 *   switch common                                              -> Teensy GND
 *
 * The two halves are read differently on purpose:
 *
 *   - The 4 switches now have a known common tied to GND, so they are read the
 *     simple way: INPUT_PULLUP, pressed = LOW.
 *   - The 5 encoder wires still have an unknown pinout, so they get the matrix
 *     treatment: drive one low, read the rest, rotate through all five. That
 *     finds contacts without knowing which wire is the common.
 *
 * WHAT TO DO
 * ----------
 *   Press each direction once  -> names each switch pin.
 *   Turn the knob slowly       -> mechanical encoder shows pairs among 2..6
 *                                 linking/opening in a repeating rhythm.
 *   Push the knob              -> one more pair among 2..6 links.
 *
 * If a full revolution produces no links among 2..6 now that the wires go
 * straight to the module, the encoder is optical and needs powering.
 *
 * Type 'd' for a dump of what is currently connected.
 */

const uint8_t ENC[]  = { 2, 3, 4, 5, 6 };
const uint8_t NENC   = sizeof(ENC) / sizeof(ENC[0]);

const uint8_t SW[]   = { 14, 15, 16, 17 };
const uint8_t NSW    = sizeof(SW) / sizeof(SW[0]);

bool linked[NENC][NENC];
bool swState[NSW];
uint32_t linkCount = 0, pressCount = 0, lastBeat = 0;

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) { }

  for (uint8_t i = 0; i < NENC; i++) {
    pinMode(ENC[i], INPUT_PULLUP);
    for (uint8_t j = 0; j < NENC; j++) linked[i][j] = false;
  }
  for (uint8_t i = 0; i < NSW; i++) {
    pinMode(SW[i], INPUT_PULLUP);
    swState[i] = false;
  }

  Serial.println();
  Serial.println("=== iDrive combo scan ===");
  Serial.print("Encoder matrix on:");
  for (uint8_t i = 0; i < NENC; i++) { Serial.print(' '); Serial.print(ENC[i]); }
  Serial.print("   |   switches on:");
  for (uint8_t i = 0; i < NSW; i++) { Serial.print(' '); Serial.print(SW[i]); }
  Serial.println();
  Serial.println("Press each direction, then turn and push the knob.\n");
  lastBeat = millis();
}

void scanSwitches() {
  for (uint8_t i = 0; i < NSW; i++) {
    bool down = (digitalRead(SW[i]) == LOW);
    if (down != swState[i]) {
      swState[i] = down;
      if (down) pressCount++;
      Serial.print(millis());
      Serial.print(down ? "  SWITCH pin " : "  release pin ");
      Serial.print(SW[i]);
      if (down) { Serial.print("   <-- press #"); Serial.print(pressCount); }
      Serial.println();
    }
  }
}

void scanEncoder() {
  for (uint8_t i = 0; i < NENC; i++) {
    pinMode(ENC[i], OUTPUT);
    digitalWrite(ENC[i], LOW);
    delayMicroseconds(60);

    for (uint8_t j = 0; j < NENC; j++) {
      if (j == i) continue;
      bool now = (digitalRead(ENC[j]) == LOW);
      if (now != linked[i][j]) {
        linked[i][j] = now;
        if (ENC[i] < ENC[j]) {
          linkCount++;
          Serial.print(millis());
          Serial.print(now ? "  LINK  " : "  OPEN  ");
          Serial.print(ENC[i]);
          Serial.print(" <-> ");
          Serial.println(ENC[j]);
        }
      }
    }
    pinMode(ENC[i], INPUT_PULLUP);
  }
}

void loop() {
  scanSwitches();
  scanEncoder();

  if (Serial.available() && Serial.read() == 'd') {
    Serial.println("-- currently connected --");
    bool any = false;
    for (uint8_t i = 0; i < NENC; i++)
      for (uint8_t j = 0; j < NENC; j++)
        if (linked[i][j] && ENC[i] < ENC[j]) {
          Serial.print("   "); Serial.print(ENC[i]);
          Serial.print(" <-> "); Serial.println(ENC[j]); any = true;
        }
    if (!any) Serial.println("   (no encoder links)");
    Serial.print("   switches down:");
    bool sdown = false;
    for (uint8_t i = 0; i < NSW; i++) if (swState[i]) { Serial.print(' '); Serial.print(SW[i]); sdown = true; }
    Serial.println(sdown ? "" : " none");
    Serial.println();
  }

  if (millis() - lastBeat > 10000) {
    lastBeat = millis();
    Serial.print("   [alive, ");
    Serial.print(linkCount);
    Serial.print(" links, ");
    Serial.print(pressCount);
    Serial.println(" presses]");
  }
}
