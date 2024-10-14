#include <wiringPi.h>
#include <iostream>

int main() {
    if (wiringPiSetup() == -1) {
        std::cout << "WiringPi setup failed" << std::endl;
        return 1;
    }

    std::cout << "WiringPi setup successful" << std::endl;

    // Test GPIO operations (adjust pin numbers as needed)
    const int testPin = 0;  // This corresponds to BCM GPIO 17 on Raspberry Pi
    pinMode(testPin, OUTPUT);

    for (int i = 0; i < 5; i++) {
        digitalWrite(testPin, HIGH);
        delay(500);
        digitalWrite(testPin, LOW);
        delay(500);
    }

    std::cout << "GPIO test completed" << std::endl;
    return 0;
}