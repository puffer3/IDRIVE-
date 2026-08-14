/*
 * idrive_watch — passive read of the now-POWERED encoder module.
 *
 * Board: Teensy 3.2 / 3.1     Tools > USB Type: Serial
 *
 * WIRING
 *   encoder V+  -> Teensy 3.3V     (was pin 23)
 *   encoder GND -> Teensy GND      (was pin 22)
 *   signals     -> pins 19, 20, 21
 *
 * This sketch only LISTENS. It never drives 19/20/21, because the module is
 * powered now and its outputs may be actively driving -- pulling against them
 * would be fighting the chip. The previous matrix sketch did drive them, which
 * was fine while everything was unpowered but is not what we want any more.
 *
 * Prints a line whenever any of the three lines changes state, and a status
 * line every 3 s so you can see the resting voltages.
 */

const uint8_t SIG[] = { 19, 20, 21 };
const uint8_t NS    = sizeof(SIG) / sizeof(SIG[0]);

uint8_t  lastState[NS];
uint32_t edges[NS];
uint32_t lastStatus = 0;

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

  // Plain INPUT, no pullup: if the IC drives its outputs we want to see them
  // as they really are.
  for (uint8_t i = 0; i < NS; i++) {
    pinMode(SIG[i], INPUT);
    edges[i] = 0;
  }
  delay(20);
  for (uint8_t i = 0; i < NS; i++) lastState[i] = digitalRead(SIG[i]);

  Serial.println();
  Serial.println("=== iDrive encoder, powered ===");
  Serial.println("Listening on 19, 20, 21. TURN THE KNOB SLOWLY.\n");
  lastStatus = millis();
}

void loop() {
  for (uint8_t i = 0; i < NS; i++) {
    uint8_t now = digitalRead(SIG[i]);
    if (now != lastState[i]) {
      lastState[i] = now;
      edges[i]++;
      Serial.print(millis());
      Serial.print("  pin ");
      Serial.print(SIG[i]);
      Serial.print(now ? " ->HIGH" : " ->LOW ");
      Serial.print("   state 19/20/21 = ");
      for (uint8_t k = 0; k < NS; k++) Serial.print(digitalRead(SIG[k]));
      Serial.print("   edges:");
      for (uint8_t k = 0; k < NS; k++) { Serial.print(' '); Serial.print(edges[k]); }
      Serial.println();
    }
  }

  if (millis() - lastStatus > 3000) {
    lastStatus = millis();
    Serial.print("   resting:");
    for (uint8_t i = 0; i < NS; i++) {
      Serial.print(' '); Serial.print(SIG[i]); Serial.print('=');
      Serial.print(readVolts(SIG[i]), 2); Serial.print('V');
    }
    Serial.print("   total edges:");
    for (uint8_t i = 0; i < NS; i++) { Serial.print(' '); Serial.print(edges[i]); }
    Serial.println();
  }
}
