#include "displayMenu.h"
#include <cstdio>

DisplayManager::DisplayManager()
    : i2cBus(std::make_shared<PicoI2C>(1, 400000)),
      oLed(std::make_shared<ssd1306os>(i2cBus)),
      params(nullptr) {
}

void DisplayManager::setParams(DisplayParams *displayParams) {
    this->params = displayParams;
}

void DisplayManager::taskEntry(void *pvParameters) {
    auto *self = static_cast<DisplayManager *>(pvParameters);
    self->displayTask(); // Run the member task loop
}

[[noreturn]] void DisplayManager::displayTask() const {
    char buf[128];

    while (true) {
        oLed->fill(0);

        // Read all values via getters
        const float co2 = params->gmpSensor->readMeasuredCO2();
        const float temp = params->hmpSensor->readTemperature();
        const float hum = params->hmpSensor->readHumidity();
        const int desiredCO2 = params->encoder->currentRotationValue();
        const float fanSpeed = params->modbusSystem->readFanSpeed();
        const bool valveState = params->modbusSystem->valveStatus();

        snprintf(buf, sizeof(buf), "CO2: %.0f ppm", co2);
        oLed->text(buf, 2, 5);

        snprintf(buf, sizeof(buf), "Set: %d ppm", desiredCO2);
        oLed->text(buf, 2, 15);

        snprintf(buf, sizeof(buf), "Temp: %.1fC", temp);
        oLed->text(buf, 2, 25);

        snprintf(buf, sizeof(buf), "Hum: %.1f%%", hum);
        oLed->text(buf, 2, 35);
        snprintf(buf, sizeof(buf), "Fan speed: %.0f%%", fanSpeed);
        oLed->text(buf, 2, 45);
        snprintf(buf, sizeof(buf), "Valve: %s", valveState ? "OPEN" : "CLOSED");
        oLed->text(buf, 2, 55);

        oLed->show();

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
