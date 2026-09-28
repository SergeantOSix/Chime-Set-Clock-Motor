#include <Arduino.h>
#include "StepperDriver.h"

// Initialize stepper motor driver for 28BYJ-48
// Pin connections: GPIO 5→IN1, GPIO 6→IN2, GPIO 7→IN3, GPIO 15→IN4
StepperDriver stepper(5, 6, 7, 15);

void setup() {
  stepper.enable();
  stepper.setDirection(0);
  stepper.setStepMode(1);
}

void loop() {
  stepper.stepBackward(2048*2, 14648);
  //stepper.stepBackward(2048*2, 1464);
}