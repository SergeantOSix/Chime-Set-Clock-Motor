#include "StepperDriver.h"
#include <Arduino.h>

// Static array definitions
const int StepperDriver::fullStepPattern[] = {
    0b0001,  // Step 0: A on
    0b0010,  // Step 1: B on
    0b0100,  // Step 2: C on
    0b1000,  // Step 3: D on
};

const int StepperDriver::halfStepPattern[] = {
    0b0001,  // Step 0: A on
    0b0011,  // Step 1: A+B on
    0b0010,  // Step 2: B on
    0b0110,  // Step 3: B+C on
    0b0100,  // Step 4: C on
    0b1100,  // Step 5: C+D on
    0b1000,  // Step 6: D on
    0b1001   // Step 7: D+A on
};

/**
 * @brief Stepper driver constructor for 28BYJ-48
 * 
 * Initializes the 4 pins for controlling a 28BYJ-48 stepper motor through ULN2003AN.
 * 
 * @param pinA Pin connected to IN1 (coil A)
 * @param pinB Pin connected to IN2 (coil B)
 * @param pinC Pin connected to IN3 (coil C)
 * @param pinD Pin connected to IN4 (coil D)
 */
StepperDriver::StepperDriver(int pinA, int pinB, int pinC, int pinD) : 
    pinA(pinA), pinB(pinB), pinC(pinC), pinD(pinD),
    _stepIndex(0), _stepMode(0), _stepDelay(1), _direction(1) {
    
    // Configure all pins as outputs
    pinMode(pinA, OUTPUT);
    pinMode(pinB, OUTPUT);
    pinMode(pinC, OUTPUT);
    pinMode(pinD, OUTPUT);
    
    // Disable motor initially (all coils off)
    disable();
}

/**
 * @brief Internal function to set coil pattern
 * 
 * @param pattern Bitmask for coil activation
 */
void StepperDriver::setCoils(int pattern) {
    digitalWrite(pinA, pattern & 0b0001);
    digitalWrite(pinB, pattern & 0b0010);
    digitalWrite(pinC, pattern & 0b0100);
    digitalWrite(pinD, pattern & 0b1000);
}

/**
 * @brief Internal function to execute one step
 * @param delayMs Delay between steps in milliseconds
 */
void StepperDriver::step(int delayMs) {
    // Apply current pattern first (before incrementing)
    if (_stepMode == 0) {
        // Full step mode
        setCoils(fullStepPattern[_stepIndex]);
        _stepIndex = (_stepIndex + _direction + 4) % 4;
    } else {
        // Half step mode
        setCoils(halfStepPattern[_stepIndex]);
        _stepIndex = (_stepIndex + _direction + 8) % 8;
    }
    delayMicroseconds(delayMs);
}

/**
 * @brief Disable all coils (motor disabled)
 */
void StepperDriver::disable() {
    setCoils(0b0000);
}

/**
 * @brief Enable all coils (motor enabled)
 */
void StepperDriver::enable() {
    // Keep current step active
    int pattern = _stepMode == 0 ? fullStepPattern[_stepIndex] : halfStepPattern[_stepIndex];
    setCoils(pattern);
}

/**
 * @brief Stop the motor and disable power
 */
void StepperDriver::stop() {
    disable();
}

/**
 * @brief Set the step mode
 * 
 * @param mode 0 for full step, 1 for half step
 */
void StepperDriver::setStepMode(int mode) {
    _stepMode = mode;
    // Reset step index to 0 when switching modes to ensure proper alignment
    _stepIndex = 0;
}

/**
 * @brief Get the current step mode
 * 
 * @return 0 for full step, 1 for half step
 */
int StepperDriver::getStepMode() const {
    return _stepMode;
}

/**
 * @brief Get the current step index
 * 
 * @return Current step index (0-3 for full step, 0-7 for half step)
 */
int StepperDriver::getStepIndex() const {
    return _stepIndex;
}

/**
 * @brief Get the current step pattern
 * 
 * @return Current active coil pattern
 */
int StepperDriver::getStepPattern() const {
    return _stepMode == 0 ? fullStepPattern[_stepIndex] : halfStepPattern[_stepIndex];
}

/**
 * @brief Reset the step index to 0
 */
void StepperDriver::resetStepIndex() {
    _stepIndex = 0;
}

/**
 * @brief Set the rotation direction
 * 
 * @param direction 1 for forward, -1 for reverse
 */
void StepperDriver::setDirection(int direction) {
    _direction = direction;
}

/**
 * @brief Set the motor speed in RPM
 * 
 * For 28BYJ-48 in full step mode:
 * - 64 steps per revolution (4 coils × 2 positions each)
 * - delay = 60000 ms / (RPM × 64 steps)
 * 
 * @param rpm Rotations per minute
 */
void StepperDriver::setSpeed(int rpm) {
    int stepsPerRevolution = 64;  // 64 steps for full step mode
    _stepDelay = 60000 / (rpm * stepsPerRevolution);
    if (_stepDelay < 1) _stepDelay = 1;
}

/**
 * @brief Step the motor at a specified speed
 * 
 * @param steps Number of steps to take
 * @param rpm Speed in RPM
 * @param forward True for forward, false for reverse
 */
void StepperDriver::stepAtSpeed(int steps, int rpm, bool forward) {
    setSpeed(rpm);
    int dir = forward ? 1 : -1;
    setDirection(dir);
    
    for (int i = 0; i < steps; i++) {
        step();
        delay(_stepDelay);
    }
}

/**
 * @brief Step the motor forward
 * 
 * @param steps Number of steps to take
 * @param delayMs Delay between steps in milliseconds
 */
void StepperDriver::stepForward(int steps, int delayMs) {
    setDirection(1);
    for (int i = 0; i < steps; i++) {
        step(delayMs);
    }
}

/**
 * @brief Step the motor backward
 * 
 * @param steps Number of steps to take
 * @param delayMs Delay between steps in milliseconds
 */
void StepperDriver::stepBackward(int steps, int delayMs) {
    setDirection(-1);
    for (int i = 0; i < steps; i++) {
        step(delayMs);
    }
}