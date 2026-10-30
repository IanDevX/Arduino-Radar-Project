#include <Stepper.h>

//STEPPER SETUP
// 28BYJ-48 is typically 2048 steps per half rotation (half-step mode)
const int stepsPerRev = 1024;

Stepper stepperMotor(stepsPerRev, 8, 10, 9, 11); 


// Sweep limits (180 degrees ≈ half rotation)
const int minPos = 0;
const int maxPos = stepsPerRev;

// Current position tracking
int currentPos = 0;
int direction = 1; // 1 = forward, -1 = backward

// Speed control (lower = smoother but slower)
int stepDelay = 3; // milliseconds between steps

//  ULTRASONIC
const int trigPin = 6;
const int echoPin = 7;

long readDistanceCM() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH, 25000); // timeout ~25ms
  long distance = duration * 0.034 / 2;

  if (distance == 0) return -1; // no reading
  return distance;
}

void setup() {
  Serial.begin(115200);

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  stepperMotor.setSpeed(15); // RPM 
}

void loop() {
  // Move one step
  stepperMotor.step(direction);
  currentPos += direction;

  // Reverse at limits
  if (currentPos >= maxPos) {
    direction = -1;
  }
  if (currentPos <= minPos) {
    direction = 1;
  }

  // Read ultrasonic distance
  long dist = readDistanceCM();

  // LIMIT distance to 20cm max
  if (dist > 20) dist = 20;

  // Optional: keep invalid reads as -1
  if (dist < 0) dist = -1;

  float angle = (float)currentPos / stepsPerRev * 180.0;

  Serial.print(angle);
  Serial.print(",");
  Serial.println(dist);

  delay(stepDelay);
}