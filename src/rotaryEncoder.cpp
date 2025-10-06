#include "rotaryEncoder.h"
#include <algorithm>

RotaryEncoder::RotaryEncoder()
    : desiredCO2(500),
      lastA(0), lastB(0),
      eeprom(i2c1, EEPROM_ADDR)
{
    gpio_init(PIN_A);
    gpio_init(PIN_B);
    gpio_set_dir(PIN_A, GPIO_IN);
    gpio_set_dir(PIN_B, GPIO_IN);
    gpio_pull_up(PIN_A);
    gpio_pull_up(PIN_B);

    readFromEEPROM();
    lastA = gpio_get(PIN_A);
    lastB = gpio_get(PIN_B);
}

int RotaryEncoder::currentRotationValue() const {
    return desiredCO2;
}

void RotaryEncoder::readFromEEPROM() {
    uint8_t buf[2];
    if (eeprom.readBlock(EEPROM_CO2_ADDR, buf, 2)) {
        desiredCO2 = (buf[0] << 8) | buf[1];
        desiredCO2 = std::clamp(desiredCO2, 200, 1500);
    } else {
        desiredCO2 = 500;
    }
}

void RotaryEncoder::writeToEEPROM() const {
    uint8_t buf[2] = {
        static_cast<uint8_t>(desiredCO2 >> 8),
        static_cast<uint8_t>(desiredCO2 & 0xFF)
    };
    Eeprom::writeBlock(EEPROM_CO2_ADDR, buf, 2);
}

void RotaryEncoder::encoderTask(void* pv) {
    auto* self = static_cast<RotaryEncoder*>(pv);
    TickType_t lastWriteTick = xTaskGetTickCount();

    while (true) {
        const int a = gpio_get(PIN_A);
        const int b = gpio_get(PIN_B);

        if (a != self->lastA) {
            if (b != a)
                self->desiredCO2 += 10;   // CW
            else
                self->desiredCO2 -= 10;   // CCW

            self->desiredCO2 = std::clamp(self->desiredCO2, 200, 1500);
            self->lastA = a;
        }

        if (xTaskGetTickCount() - lastWriteTick > pdMS_TO_TICKS(2000)) {
            self->writeToEEPROM();
            lastWriteTick = xTaskGetTickCount();
        }

        vTaskDelay(pdMS_TO_TICKS(5)); // poll every 5 ms
    }
}
