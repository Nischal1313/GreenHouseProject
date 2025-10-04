#include "displayMenu.h"

DisplayManager::DisplayManager(const DisplayParams& params)
    : i2cBus(params.i2cBus),
      oled(params.oled),
      gmpSensor(params.gmpSensor),
      hmpSensor(params.hmpSensor),
      modbusSystem(params.modbusSystem),
      encoder(params.encoder),
      valve(params.valve) {}

[[noreturn]] void DisplayManager::displayTask() const {
    char buf[128];

    while (true) {
        oled->fill(0);

        const float co2 = gmpSensor->readMeasuredCO2();
        const float hum = hmpSensor->readHumidity();
        const float temp = hmpSensor->readTemperature();

        const int desiredCO2 = encoder->currentRotationValue();
        const float fanSpeed = modbusSystem->readFanSpeed();
        const bool fanRunning = modbusSystem->isFanRunning();
        const bool valveState = valve->valveStatus();

        snprintf(buf, sizeof(buf), "CO2: %.1f ppm", co2);
        oled->text(buf, 5, 5);

        snprintf(buf, sizeof(buf), "Desired: %d ppm", desiredCO2);
        oled->text(buf, 5, 15);

        snprintf(buf, sizeof(buf), "Temp: %.1f Hum: %.1f %%", temp, hum);
        oled->text(buf, 5, 25);

        snprintf(buf, sizeof(buf), "Fan: %s", fanRunning ? "ON" : "OFF");
        oled->text(buf, 5, 35);

        snprintf(buf, sizeof(buf), "Speed: %.0f%%", fanSpeed);
        oled->text(buf, 5, 45);

        snprintf(buf, sizeof(buf), "Valve: %s", valveState ? "ON" : "OFF");
        oled->text(buf, 5, 55);

        oled->show();

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
