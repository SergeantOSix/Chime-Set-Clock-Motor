#ifndef STEPPER_DRIVER_H
#define STEPPER_DRIVER_H

/**
 * @file StepperDriver.h
 * @brief Stepper motor driver class for 28BYJ-48 with ULN2003AN style drivers
 * 
 * This class provides a simple interface for controlling 28BYJ-48 stepper motors
 * through ULN2003AN or similar driver boards.
 * 
 * Configuration: 4 GPIO pins (IN1-4)
 * - GPIO 2 → IN1 (A) - bit 0 (0b0001)
 * - GPIO 3 → IN2 (B) - bit 1 (0b0010)
 * - GPIO 4 → IN3 (C) - bit 2 (0b0100)
 * - GPIO 5 → IN4 (D) - bit 3 (0b1000)
 */

class StepperDriver {
private:
    int pinA;  // IN1
    int pinB;  // IN2
    int pinC;  // IN3
    int pinD;  // IN4
    
    // Coil activation patterns for different step modes
    static const int fullStepPattern[];
    static const int halfStepPattern[];
    
    int _stepIndex;
    int _stepMode;  // 0 = full step, 1 = half step
    int _stepDelay;
    int _direction;  // 1 = forward, -1 = reverse
    
    void setCoils(int pattern);
    
    public:
        /**
         * @brief Initialize the stepper driver
         * @param pinA Pin connected to IN1 (coil A)
         * @param pinB Pin connected to IN2 (coil B)
         * @param pinC Pin connected to IN3 (coil C)
         * @param pinD Pin connected to IN4 (coil D)
         */
        StepperDriver(int pinA, int pinB, int pinC, int pinD);
    
        /**
         * @brief Set the step mode
         * @param mode 0 for full step, 1 for half step
         */
        void setStepMode(int mode);
    
        /**
                 * @brief Get the current step mode
                 * @return 0 for full step, 1 for half step
         */
                int getStepMode() const;
    
                /**
                 * @brief Get the current step index
                 * @return Current step index (0-3 for full step, 0-7 for half step)
                 */
                int getStepIndex() const;
    
                /**
                 * @brief Get the current step pattern
                 * @return Current active coil pattern
                 */
                int getStepPattern() const;
    
                /**
                 * @brief Reset the step index to 0
                 */
                void resetStepIndex();
    
                /**
                 * @brief Set the rotation direction
                 * @param direction 1 for forward, -1 for reverse
                 */
                void setDirection(int direction);
    
        /**
         * @brief Step the motor one position
         * @param delayMs Delay between steps in milliseconds
         */
        void step(int delayMs = 1);
    
        /**
         * @brief Step the motor forward
         * @param steps Number of steps to take
         * @param delayMs Delay between steps in milliseconds
         */
        void stepForward(int steps, int delayMs = 1);
    
        /**
         * @brief Step the motor backward
         * @param steps Number of steps to take
         * @param delayMs Delay between steps in milliseconds
         */
        void stepBackward(int steps, int delayMs = 1);
    
        /**
         * @brief Set the motor speed in RPM
         * @param rpm Rotations per minute
         */
        void setSpeed(int rpm);
    
        /**
         * @brief Step the motor at a specified speed
         * @param steps Number of steps to take
         * @param rpm Speed in RPM
         * @param forward True for forward, false for reverse
         */
        void stepAtSpeed(int steps, int rpm, bool forward = true);
    
        /**
         * @brief Enable all coils (motor enabled)
         */
        void enable();
    
        /**
         * @brief Disable all coils (motor disabled)
         */
        void disable();
    
    /**
         * @brief Stop the motor and disable power
         */
        void stop();
    };

    #endif // STEPPER_DRIVER_H