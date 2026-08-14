/*
 * idrive_pinscan — figure out an unknown BMW iDrive knob pinout empirically.
 *
 * Board: Teensy 3.2 / 3.1     Tools > USB Type: Serial
 *
 * Wire the 5 unknown CN2 wires to pins 2..6, and the 4 direction-switch wires
 * to pins 7..10. Wire the switch common (and, if you find one, the encoder
 * common) to Teensy GND. Nothing else. No external supply, no resistors.
 *
 * Every pin below is driven INPUT_PULLUP, so an unconnected pin reads HIGH and
 * a pin shorted to GND reads LOW. Teensy 3.1 pins are 5V tolerant, so even if
 * one of these wires turns out to carry 5V from somewhere, you will not damage
 * the board.
 *
 * Then just open the Serial Monitor and play with the knob:
 *
 *   - TURN it slowly. Two of pins 2..6 should toggle in a repeating
 *     out-of-phase pattern -> those are encoder A and B, and the encoder is a
 *     plain MECHANICAL quadrature type. You are done; note the two pins.
 *   - PUSH it. Whichever pin toggles only on press is the push switch.
 *   - TILT it 4 ways to confirm which of 7..10 is up/down/left/right.
 *
 * If you turn the knob and NOTHING on 2..6 ever toggles, the encoder is
 * OPTICAL (or Hall) and needs to be powered before it will output anything.
 * See the note at the bottom of this file before going further.
 */

const uint8_t PINS[] = { 2, 3, 4, 5, 6, 7, 8, 9, 10 };
const uint8_t N = sizeof(PINS) / sizeof(PINS[0]);

uint8_t   last[N];
uint32_t  changes[N];
uint32_t  lastReport;

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 4000) { /* wait for the monitor, but don't hang */ }

  for (uint8_t i = 0; i < N; i++) {
    pinMode(PINS[i], INPUT_PULLUP);
    changes[i] = 0;
  }
  delay(50);                                  // let the pullups settle
  for (uint8_t i = 0; i < N; i++) last[i] = digitalRead(PINS[i]);

  Serial.println();
  Serial.println("iDrive pin scan ready.");
  Serial.println("Turn / push / tilt the knob and watch which pins move.");
  Serial.print  ("Resting state: ");
  printState();
  Serial.println("Type 'r' at any time to reset the change counters.\n");
  lastReport = millis();
}

void printState() {
  for (uint8_t i = 0; i < N; i++) {
    Serial.print(PINS[i]);
    Serial.print(digitalRead(PINS[i]) ? ":H " : ":L ");
  }
  Serial.println();
}

void loop() {
  // Report any pin that changed, with a timestamp so you can see phase
  // relationships between A and B while turning slowly.
  for (uint8_t i = 0; i < N; i++) {
    uint8_t now = digitalRead(PINS[i]);
    if (now != last[i]) {
      last[i] = now;
      changes[i]++;
      Serial.print(millis());
      Serial.print("  pin ");
      Serial.print(PINS[i]);
      Serial.print(now ? "  ->HIGH   " : "  ->LOW    ");
      Serial.print("[");
      Serial.print(changes[i]);
      Serial.println(" edges]");
    }
  }

  // Every 5 s, a compact summary of which pins are actually alive.
  if (millis() - lastReport > 5000) {
    lastReport = millis();
    bool any = false;
    for (uint8_t i = 0; i < N; i++) if (changes[i]) any = true;
    if (any) {
      Serial.print("  ...active pins so far: ");
      for (uint8_t i = 0; i < N; i++) {
        if (changes[i]) { Serial.print(PINS[i]); Serial.print("("); Serial.print(changes[i]); Serial.print(") "); }
      }
      Serial.println();
    }
  }

  if (Serial.available() && Serial.read() == 'r') {
    for (uint8_t i = 0; i < N; i++) changes[i] = 0;
    Serial.println("-- counters reset --");
  }
}

/*
 * EXPECT NOTHING TO TOGGLE — AND THAT IS INFORMATION
 * --------------------------------------------------
 * The board's own circuitry says this encoder is almost certainly OPTICAL
 * (a photointerrupter), not mechanical contacts. Right next to CN2 sit a
 * 100 ohm resistor (marked 101) and a transistor T5 -- that is an IR LED
 * current limiter with an enable switch, which a contact encoder has no use
 * for -- alongside roughly three 1k/10k divider pairs, i.e. three
 * open-collector signal channels. Three channels + one LED + one ground is
 * exactly the 5 pins on CN2. So the likely map is:
 *
 *     LED anode (+)   /   GND   /   out A   /   out B   /   out PUSH
 *
 * German BMW forums back this up: they call the iDrive rotation sensor a
 * "Lichtschranke" (light barrier) and the standard repair is cleaning it with
 * a brush, which is not something you do to a contact encoder.
 *
 * If this scan shows no movement at all while turning, that CONFIRMS optical.
 * Proceed like this:
 *
 *   1. WARNING FIRST -- check for a haptic motor. BMW iDrive controllers of
 *      this era can contain a PWM-driven DC motor for force feedback. Ohm
 *      every pin pair: anything reading ~5-50 ohms resistive is a motor
 *      winding, NOT an LED. Do not feed it a logic supply.
 *   2. Diode mode across all pin pairs, both polarities. ~0.9-1.3 V one way
 *      and open the other = the IR LED; red probe sits on the anode. Its
 *      cathode side is usually module GND. The remaining two or three pins
 *      are phototransistor outputs.
 *   3. Drive the LED anode from 5 V through 150-220 ohms to start (the board
 *      originally used ~100 ohms; match that once you have measured it).
 *      Confirm it lights: a phone camera in a dark room renders IR as a faint
 *      violet-white, or look for ~1.1 V across the LED.
 *   4. Put EXTERNAL 10k pull-ups to 3.3 V on each output. The Teensy's
 *      internal pullups used above are ~20-50k, which is too weak to pull a
 *      phototransistor cleanly -- this is the step people skip and then
 *      conclude the encoder is dead.
 *   5. Re-run this scan. Two outputs should now swing 90 degrees out of phase
 *      as you turn; the third stays put until you press.
 *
 * Teensy 3.1 digital pins are 5V tolerant, so 5 V open-collector outputs are
 * safe to read directly. (The exceptions are the analog-only pins A10-A14,
 * AREF, Program and Reset -- do not put 5 V on those.)
 */
