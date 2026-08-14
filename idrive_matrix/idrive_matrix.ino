/*
 * idrive_matrix — find every contact closure between unknown wires.
 *
 * Board: Teensy 3.2 / 3.1     Tools > USB Type: Serial
 *
 * WHY THIS EXISTS
 * ---------------
 * The plain pullup scan can only see a contact if one side of it is already
 * tied to ground. With a bundle of unknown wires you don't know which one is
 * the common, so nothing ever moves and you learn nothing.
 *
 * This sketch removes that assumption. It walks every pin in turn: drives ONE
 * pin LOW as an output, leaves all the others as pulled-up inputs, and reads
 * them. If wire j is connected to wire i through a switch or an encoder
 * contact, pin j reads LOW while pin i is the one being driven. Repeat for
 * every pin and you have a full continuity map, refreshed hundreds of times a
 * second — no ground wire needed, and no need to know which wire is common.
 *
 * WIRING
 * ------
 *   5 encoder/CN2 wires  -> pins 2, 3, 4, 5, 6
 *   8 switch wires       -> pins 14..21
 * Nothing to GND. Nothing to 3.3V. Just the wires.
 *
 * (If you only have some of these connected, that's fine — unconnected pins
 * simply never link to anything.)
 *
 * WHAT YOU'LL SEE
 * ---------------
 *   LINK  3 <-> 5     a connection just closed
 *   OPEN  3 <-> 5     it just opened again
 *
 *   - Press a direction: exactly one pair in 14..21 should LINK. That names
 *     that switch's two wires. Four presses -> all four switches mapped.
 *   - Turn the knob: if it is a MECHANICAL encoder you'll see pairs among
 *     2..6 linking and opening in a repeating rhythm, and the wire that
 *     appears in every pair is the common.
 *   - Push the knob: one more pair among 2..6 links.
 *
 * If you turn a full revolution and NOTHING among pins 2..6 ever links, then
 * the encoder really is optical and needs powering — that conclusion is now
 * trustworthy, because this test no longer depends on a ground wire.
 *
 * Safety: only one pin is ever an output, and only ever driven LOW. Against a
 * passive contact or an open-collector output that is harmless.
 */

const uint8_t PINS[] = { 2, 3, 4, 5, 6, 14, 15, 16, 17, 18, 19, 20, 21 };
const uint8_t N = sizeof(PINS) / sizeof(PINS[0]);

bool linked[N][N];        // linked[i][j] : pin i driven, pin j read LOW
uint32_t linkCount = 0;
uint32_t lastBeat = 0;

void allInputs() {
  for (uint8_t i = 0; i < N; i++) pinMode(PINS[i], INPUT_PULLUP);
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) { }

  allInputs();
  for (uint8_t i = 0; i < N; i++)
    for (uint8_t j = 0; j < N; j++) linked[i][j] = false;

  Serial.println();
  Serial.println("=== iDrive continuity matrix ===");
  Serial.print("Scanning pins:");
  for (uint8_t i = 0; i < N; i++) { Serial.print(' '); Serial.print(PINS[i]); }
  Serial.println();
  Serial.println("Turn the knob, push it, press each direction.");
  Serial.println("Type 'd' for a full dump of what is currently connected.\n");
  lastBeat = millis();
}

void loop() {
  for (uint8_t i = 0; i < N; i++) {
    // Drive pin i low, everything else stays pulled up.
    pinMode(PINS[i], OUTPUT);
    digitalWrite(PINS[i], LOW);
    delayMicroseconds(60);                 // let the line settle

    for (uint8_t j = 0; j < N; j++) {
      if (j == i) continue;
      bool now = (digitalRead(PINS[j]) == LOW);
      if (now != linked[i][j]) {
        linked[i][j] = now;
        // Only report one direction of each pair to halve the noise.
        if (PINS[i] < PINS[j]) {
          linkCount++;
          Serial.print(millis());
          Serial.print(now ? "  LINK  " : "  OPEN  ");
          Serial.print(PINS[i]);
          Serial.print(" <-> ");
          Serial.println(PINS[j]);
        }
      }
    }

    pinMode(PINS[i], INPUT_PULLUP);        // release before moving on
  }

  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'd') {
      Serial.println("-- currently connected --");
      bool any = false;
      for (uint8_t i = 0; i < N; i++)
        for (uint8_t j = 0; j < N; j++)
          if (linked[i][j] && PINS[i] < PINS[j]) {
            Serial.print("   "); Serial.print(PINS[i]);
            Serial.print(" <-> "); Serial.println(PINS[j]);
            any = true;
          }
      if (!any) Serial.println("   (nothing)");
      Serial.println();
    }
  }

  // Slow heartbeat so it is obvious the scan is alive even when idle.
  if (millis() - lastBeat > 10000) {
    lastBeat = millis();
    Serial.print("   [alive, ");
    Serial.print(linkCount);
    Serial.println(" link events so far]");
  }
}
