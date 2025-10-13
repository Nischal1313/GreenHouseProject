#include "sensor_handler.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "FreeRTOS.h"
#include "task.h"

SensorHandler::SensorHandler(const SemaphoreHandle_t mutex, const std::shared_ptr<RotaryEncoder> &encoderPtr)
  : mutex(mutex), encoder(encoderPtr) {
  // Initialize UART for Modbus communication
  auto uart = std::make_shared<PicoOsUart>(1, 4, 5, 9600, 8, 256, 256);
  auto modbusClient = std::make_shared<ModbusClient>(uart);

  // Initialize all sensors with shared Modbus client
  gmpSensor = std::make_shared<GMP252>(modbusClient, mutex);
  hmpSensor = std::make_shared<HMP60>(modbusClient, mutex);
  fan = std::make_shared<ModbusMIO>(modbusClient, mutex);
  valve = std::make_shared<VALVE>();
}

SensorValues SensorHandler::getReadings() const {
  SensorValues vals{};
  vals.co2 = gmpSensor->readMeasuredCO2();
  vals.temperature = hmpSensor->readTemperature();
  vals.humidity = hmpSensor->readHumidity();
  vals.fanSpeed = fan->readFanSpeed();
  vals.valveOpen = valve->valveStatus();
  vals.targetCo2 = targetCo2;
  return vals;
}


void SensorHandler::handleValveAndFanLogic(const float co2Lvl, const int desiredCo2Lvl) const {
  const int diff = desiredCo2Lvl - static_cast<int>(co2Lvl);

  if (std::abs(diff) > ACCEPTED_RANGE) {
    // CO2 too high → ventilation
    if (diff < 0) {
      fan->setFanSpeed(FULL_SPEED);
      valve->closeValve();
      return;
    }

    // CO2 too low → inject CO2
    // Scaling: MAX_VALVE_OPEN_TIME = 2s → +1000 ppm
    int openTimeMs = (diff * static_cast<int>(MAX_VALVE_OPEN_TIME)) / 1000;
    openTimeMs = std::clamp(openTimeMs, 50, static_cast<int>(MAX_VALVE_OPEN_TIME));

    // Open valve and inject CO2
    fan->setFanSpeed(IDLE_SPEED);
    valve->openValve();
    vTaskDelay(pdMS_TO_TICKS(openTimeMs));
    valve->closeValve();

    // Wait for CO2 to settle after injection
    vTaskDelay(pdMS_TO_TICKS(VALVE_IDLE_TIME));

    // Set fan to idle after injection
    fan->setFanSpeed(IDLE_SPEED);
  } else {
    // Within acceptable range - maintain idle
    fan->setFanSpeed(IDLE_SPEED);
    valve->closeValve();
  }
}


void SensorHandler::updateFromEncoder() {
  // Check for encoder rotation events
  const bool cwRotation = encoder->rotatedCW();
  const bool ccwRotation = encoder->rotatedCCW();

  if (cwRotation) {
    targetCo2 = std::min(targetCo2 + 5, static_cast<int>(MAX_CO2));
  }
  if (ccwRotation) {
    targetCo2 = std::max(targetCo2 - 5, static_cast<int>(MIN_CO2));
  }
}


void SensorHandler::updateControl() const {
  // Read CO2 sensor
  const float currentCo2 = gmpSensor->readMeasuredCO2();
  // Validate reading before controlling
  if (!std::isnan(currentCo2)) {
    // printf("Current CO2: %.1f | Target CO2: %d\n", currentCo2, targetCo2);
    handleValveAndFanLogic(currentCo2, targetCo2);
  } else {
    printf("[SensorHandler] CO2 read failed.\n");
  }
}

[[noreturn]] void SensorHandler::controlLoop() {
  while (true) {
    // getReadings();
    updateControl();
    updateFromEncoder();
    vTaskDelay(pdMS_TO_TICKS(60));
  }
}

void SensorHandler::controlTask(void *pvParameters) {
  auto *self = static_cast<SensorHandler *>(pvParameters);
  self->controlLoop();
}
