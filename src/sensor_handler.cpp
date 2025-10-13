#include "sensor_handler.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "FreeRTOS.h"
#include "task.h"

SensorHandler::SensorHandler(SetpointManager *spManager, const SemaphoreHandle_t mutex)
  : setpointManager(spManager), mutex(mutex) {

  // Initialize UART for Modbus communication
  auto uart = std::make_shared<PicoOsUart>(1, 4, 5, 9600, 8, 256, 256);
  auto modbusClient = std::make_shared<ModbusClient>(uart);

  // Initialize all sensors with shared Modbus client
  gmpSensor = std::make_shared<GMP252>(modbusClient, mutex);
  hmpSensor = std::make_shared<HMP60>(modbusClient, mutex);
  fan = std::make_shared<ModbusMIO>(modbusClient, mutex);
  valve = std::make_shared<VALVE>();
}

void SensorHandler::readSensors() {
  SensorValues vals{};
  vals.co2 = gmpSensor->readMeasuredCO2();
  vals.temperature = hmpSensor->readTemperature();
  vals.humidity = hmpSensor->readHumidity();
  vals.fanSpeed = fan->readFanSpeed();
  vals.valveOpen = valve->valveStatus();

  MutexGuard lock(mutex);
  if (!lock.owns_lock()) return;
  latestReadings = vals;
}

SensorValues SensorHandler::getLatestReadings() const {
  MutexGuard lock(mutex);
  if (!lock.owns_lock()) return SensorValues{};
  return latestReadings;
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

void SensorHandler::updateControl() {
  // Get current setpoint (handles cloud vs local priority)
  const int targetCo2 = setpointManager->getEffectiveTarget();

  // Get current CO2 reading
  const float currentCo2 = gmpSensor->readMeasuredCO2();

  // Validate reading before controlling
  if (!std::isnan(currentCo2)) {
    handleValveAndFanLogic(currentCo2, targetCo2);
  } else {
    printf("SensorHandler: CO2 read failed.\n");
  }
}

[[noreturn]] void SensorHandler::controlLoop() {
  while (true) {
    // Read all sensors
    readSensors();

    // Update control based on setpoint
    updateControl();

    // Wait before next iteration
    vTaskDelay(pdMS_TO_TICKS(CONTROL_LOOP_DELAY));
  }
}

void SensorHandler::controlTask(void *pvParameters) {
  auto *self = static_cast<SensorHandler *>(pvParameters);
  self->controlLoop();
}
