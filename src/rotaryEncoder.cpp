#include "rotaryEncoder.h"
#include <algorithm>
#include <cstdio>

#define EEPROM_CO2_ADDR 0x00
extern volatile bool isInMainMenu;

RotaryEncoder::RotaryEncoder()
    : desiredCO2(500),
      desiredRotation(0),
      lastA(0), lastB(0),
      eeprom(i2c0, 0x50, 2) // EEPROM on I2C0
{
    // Initialize EEPROM I2C bus
    i2c_init(i2c0, 400000);
    gpio_set_function(16, GPIO_FUNC_I2C);
    gpio_set_function(17, GPIO_FUNC_I2C);
    gpio_pull_up(16);
    gpio_pull_up(17);

    // Encoder GPIO setup
    gpio_init(PIN_A);
    gpio_init(PIN_B);
    gpio_set_dir(PIN_A, GPIO_IN);
    gpio_set_dir(PIN_B, GPIO_IN);
    gpio_pull_up(PIN_A);
    gpio_pull_up(PIN_B);

    // Read stored CO2 target
    readFromEEPROM();

    lastA = gpio_get(PIN_A);
    lastB = gpio_get(PIN_B);
}

int RotaryEncoder::currentRotationValue() const {
    if (isInMainMenu)
        return desiredCO2;
    else
        return desiredRotation;
}

void RotaryEncoder::readFromEEPROM() {
    uint8_t buf[2] = {0};
    if (eeprom.readBlock(EEPROM_CO2_ADDR, buf, 2)) {
        desiredCO2 = (buf[0] << 8) | buf[1];
        desiredCO2 = std::clamp(desiredCO2, 200, 1500);
    } else {
        desiredCO2 = 500;
    }
}

void RotaryEncoder::writeToEEPROM() {
    uint8_t buf[2] = {
        static_cast<uint8_t>(desiredCO2 >> 8),
        static_cast<uint8_t>(desiredCO2 & 0xFF)
    };
    eeprom.writeBlock(EEPROM_CO2_ADDR, buf, 2);
}

void RotaryEncoder::encoderTask(void* pv) {
    auto* self = static_cast<RotaryEncoder*>(pv);
    TickType_t lastWriteTick = xTaskGetTickCount();

    while (true) {
        const int a = gpio_get(PIN_A);
        const int b = gpio_get(PIN_B);

        if (a != self->lastA) {
            // Determine direction
            bool clockwise = (b != a);

            if (isInMainMenu) {
                // Adjust CO₂ value
                self->desiredCO2 += clockwise ? 10 : -10;
                self->desiredCO2 = std::clamp(self->desiredCO2, 200, 1500);
            } else {
                // Adjust rotation value
                self->desiredRotation += clockwise ? 10 : -10;

                // Optional: make it "wrap" around smoothly
                if (self->desiredRotation > 5000) self->desiredRotation = -5000;
                if (self->desiredRotation < -5000) self->desiredRotation = 5000;
            }

            self->lastA = a;
        }

        // Only save EEPROM periodically when in main menu
        if (isInMainMenu && (xTaskGetTickCount() - lastWriteTick > pdMS_TO_TICKS(4000))) {
            self->writeToEEPROM();
            lastWriteTick = xTaskGetTickCount();
        }

        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
