#include <AccelStepper.h>

// Test comment

// ---------------- STEPPER SETUP ----------------
// 28BYJ-48 via ULN2003 driver board, half-step mode: 4096 steps per full revolution
const int STEPS_PER_REV = 4096;
const int SWEEP_STEPS = STEPS_PER_REV / 2; // 180 degrees = 2048 steps

AccelStepper stepper(AccelStepper::HALF4WIRE, 8, 10, 9, 11);

// ---------------- ULTRASONIC ----------------
const int trigPin = 6;
const int echoPin = 7;
const int MAX_RANGE_CM = 40;

long lastSampleStep = 0;
const int SAMPLE_EVERY_N_STEPS = 4; // read distance every N steps instead of every step

// ---------------- BUZZER ----------------
const int buzzerPin = 12;

// zone boundaries - match the pygame radar's range rings (40cm, 27cm, 13cm)
const int ZONE_NEAR_CM = 13; // 0-13cm  -> near zone
const int ZONE_MID_CM  = 27; // 13-27cm -> mid zone
                              // 27-40cm -> far zone

// hysteresis buffer - once in a closer zone, distance must grow past
// (boundary + HYSTERESIS_CM) before switching back out, so a reading
// sitting right on a boundary doesn't flicker between tones
const int HYSTERESIS_CM = 2;

long lastDist = -1; // most recent raw distance reading, -1 = nothing in range

// current zone: 0 = calm (nothing), 1 = far, 2 = mid, 3 = near
int currentZone = 0;

bool buzzerOn = false; // tracks whether tone() is currently active, to avoid restarting it every loop

// ---------------- RUN/PAUSE STATE ----------------
// Controlled by the Python side: space bar sends 'T' to toggle this.
bool running = false;

long readDistanceCM() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // timeout scaled roughly to MAX_RANGE_CM (speed of sound ~0.034 cm/us, round trip)
  long duration = pulseIn(echoPin, HIGH, 2500UL);
  if (duration == 0) return -1; // no echo / out of range

  return duration * 0.034 / 2;
}

// Determines which zone we're in, with hysteresis so boundary readings
// don't cause rapid flicker between tones. Called once per new sensor reading.
void updateZone(long dist) {
  if (dist < 0) {
    currentZone = 0; // calm - nothing detected
    return;
  }

  switch (currentZone) {
    case 0: // was calm -> pick a zone fresh, no hysteresis needed entering from nothing
    case 1: // was far
      if (dist <= ZONE_NEAR_CM) currentZone = 3;
      else if (dist <= ZONE_MID_CM) currentZone = 2;
      else currentZone = 1;
      break;

    case 2: // was mid -> needs to clear buffer to go back to far, or drop into near
      if (dist <= ZONE_NEAR_CM) currentZone = 3;
      else if (dist > ZONE_MID_CM + HYSTERESIS_CM) currentZone = 1;
      // else stays mid
      break;

    case 3: // was near -> needs to clear buffer to go back to mid
      if (dist > ZONE_NEAR_CM + HYSTERESIS_CM) {
        currentZone = (dist <= ZONE_MID_CM) ? 2 : 1;
      }
      // else stays near
      break;
  }
}

// Non-blocking buzzer update. Call this every loop() iteration.
void updateBuzzer() {
  unsigned long now = millis();

  unsigned int freq;
  unsigned int period;
  const unsigned int BEEP_ON_MS = 60; // same beep length in every zone

  switch (currentZone) {
    case 0: // calm - nothing detected
      freq = 1200;
      period = 700;
      break;

    case 1: // far zone (27-40cm) - same relaxed rhythm as calm, just a different note
      freq = 1000;
      period = 700;
      break;

    case 2: // mid zone (13-27cm) - distinct tone, a little faster
      freq = 1400;
      period = 500;
      break;

    case 3: // near zone (0-13cm) - distinct tone, a little faster still, but capped
      freq = 1800;
      period = 380; // fast enough to feel urgent, still clearly separate beeps
      break;
  }

  unsigned long phase = now % period;
  if (phase < BEEP_ON_MS) {
    if (!buzzerOn) {
      tone(buzzerPin, freq);
      buzzerOn = true;
    }
  } else {
    if (buzzerOn) {
      noTone(buzzerPin);
      buzzerOn = false;
    }
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(buzzerPin, OUTPUT);

  stepper.setMaxSpeed(600);      // steps/sec - raise for a faster sweep
  stepper.setAcceleration(400);  // steps/sec^2 - smooths out the direction reversals
  stepper.moveTo(SWEEP_STEPS);
}

void loop() {
  // ---------------- CHECK FOR START/PAUSE COMMAND ----------------
  if (Serial.available() > 0) {
    char cmd = Serial.read();
    if (cmd == 'T') {
      running = !running;
    }
  }

  // stay idle (motor holds position, no data sent) until toggled on
  if (!running) {
    if (buzzerOn) {
      noTone(buzzerPin);
      buzzerOn = false;
    }
    return;
  }

  stepper.run(); // non-blocking; must be called as often as possible

  // reverse sweep direction once each end is reached
  if (stepper.distanceToGo() == 0) {
    if (stepper.currentPosition() >= SWEEP_STEPS) {
      stepper.moveTo(0);
    } else {
      stepper.moveTo(SWEEP_STEPS);
    }
  }

  long currentStep = stepper.currentPosition();

  // only take a distance reading (and send data) every N steps,
  // so the sweep speed doesn't depend on how long pulseIn() takes
  if (abs(currentStep - lastSampleStep) >= SAMPLE_EVERY_N_STEPS) {
    lastSampleStep = currentStep;

    long dist = readDistanceCM();
    if (dist < 0 || dist > MAX_RANGE_CM) dist = -1;
    lastDist = dist;

    updateZone(dist); // decides which of the 4 zones we're in, with hysteresis

    float angle = (float)currentStep / SWEEP_STEPS * 180.0;

    Serial.print(angle, 1);
    Serial.print(",");
    Serial.println(dist);
  }

  updateBuzzer(); // called every loop iteration so the beep timing stays smooth
}