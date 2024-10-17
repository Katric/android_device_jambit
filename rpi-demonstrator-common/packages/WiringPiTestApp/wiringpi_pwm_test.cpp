#include <wiringPi.h>
#include <softPwm.h>
#include <iostream>
#include <unistd.h>

int main() {
    if (wiringPiSetup() == -1) {
        std::cout << "WiringPi setup failed" << std::endl;
        return 1;
    }
    std::cout << "WiringPi setup successful" << std::endl;

    // Define pins for LEDs
    const int ledPins[3] = {0, 1, 2};  // WiringPi pin numbers

    // Initialize software PWM on each pin
    for (int i = 0; i < 3; i++) {
        if (softPwmCreate(ledPins[i], 0, 100) != 0) {
            std::cout << "Failed to set up software PWM on pin " << ledPins[i] << std::endl;
            return 1;
        }
    }

    std::cout << "Software PWM initialized on pins 0, 1, and 2" << std::endl;

    // Cycle through brightness levels
    for (int cycle = 0; cycle < 3; cycle++) {
        std::cout << "Starting cycle " << cycle + 1 << std::endl;

        // Fade in
        for (int value = 0; value <= 100; value += 5) {
            for (int i = 0; i < 3; i++) {
                softPwmWrite(ledPins[i], value);
            }
            usleep(50000);  // 50ms delay
        }

        // Fade out
        for (int value = 100; value >= 0; value -= 5) {
            for (int i = 0; i < 3; i++) {
                softPwmWrite(ledPins[i], value);
            }
            usleep(50000);  // 50ms delay
        }
    }

    // Clean up
    for (int i = 0; i < 3; i++) {
        softPwmStop(ledPins[i]);
    }

    std::cout << "PWM test completed" << std::endl;
    return 0;
}