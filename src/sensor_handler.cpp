#include "sensor_handler.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "FreeRTOS.h"
#include "task.h"
#include "eeprom/eeprom.h"

SensorHandler::SensorHandler(const SemaphoreHandle_t mutex, const
                             std::shared_ptr<RotaryEncoder> &
                             encoderPtr,
                             const SemaphoreHandle_t eepromMutex,
                             Eeprom &eeprom,
                             const std::shared_ptr<InputManager>
                             &inputManager
)
  : encoder(encoderPtr), eeprom(&eeprom), mutex(mutex),
    eepromMutex(eepromMutex), inputManager(inputManager) {
  auto uart = std::make_shared<PicoOsUart>(1, 4, 5,
                                           9600, 8,
                                           256, 256);

  auto modbusClient = std::make_shared<ModbusClient>(uart);


  gmpSensor = std::make_shared<GMP252>(modbusClient, mutex);
  hmpSensor = std::make_shared<HMP60>(modbusClient, mutex);
  fan = std::make_shared<ModbusMIO>(modbusClient, mutex);
  valve = std::make_shared<VALVE>();
  copyValueFromEEPROM(EEPROM_CO2_ADDR, targetCo2);
  inMainMenu = true;
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

void SensorHandler::copyValueFromEEPROM(const uint16_t addr,
                                        int &valueToWriteTo) const {
  const MutexGuard lock(eepromMutex);
  uint8_t buf[2] = {0};
  if (eeprom->readBlock(addr, buf, 2)) {
    const int checkingValue = (buf[0] << 8) | buf[1];
    if (checkingValue < 200 or checkingValue > 1500) {
      return;
    }
    valueToWriteTo = checkingValue;
  }
}

void SensorHandler::emptyValueFromEEPROM() const {
  constexpr uint8_t buf[2] = {
    static_cast<uint8_t>(0 >> 8),
    static_cast<uint8_t>(0 & 0xFF)
  };
  eeprom->writeBlock(EEPROM_CO2_CLOUD_ADDR, buf, 2);
}

void SensorHandler::writeToEEPROM() const {
  static absolute_time_t lastWriteTime = {0};
  const int64_t now = to_ms_since_boot(get_absolute_time());
  const int64_t elapsed = now - to_ms_since_boot(lastWriteTime);

  if (elapsed < 15'000) return;

  const MutexGuard lock(eepromMutex);
  const uint8_t buf[2] = {
    static_cast<uint8_t>(targetCo2 >> 8),
    static_cast<uint8_t>(targetCo2 & 0xFF)
  };
  if (eeprom->writeBlock(EEPROM_CO2_ADDR, buf, 2))
    lastWriteTime = get_absolute_time();
}


void SensorHandler::handleValveAndFanLogic(
  const float co2Lvl, const int desiredCo2Lvl) const {
  const int diff = desiredCo2Lvl - static_cast<int>(co2Lvl);
  const uint32_t now = to_ms_since_boot(get_absolute_time());
  // Handle ongoing valve open period
  if (valveActive) {
    if (now - lastValveActionTime >= valveOpenDuration) {
      valve->closeValve();
      fan->setFanSpeed(IDLE_SPEED);
      valveActive = false;
      lastValveActionTime = now;
    }
    return;
  }


  if (now - lastValveActionTime < VALVE_IDLE_TIME) return;

  if (std::abs(diff) > ACCEPTED_RANGE) {
    if (diff < 0) {
      fan->setFanSpeed(FULL_SPEED);
      valve->closeValve();
    } else {
      int openTimeMs = (diff * static_cast<int>(MAX_VALVE_OPEN_TIME))
                       / 1000;
      openTimeMs = std::clamp(openTimeMs, 50,
                              static_cast<int>(MAX_VALVE_OPEN_TIME));
      valve->openValve();
      fan->setFanSpeed(IDLE_SPEED);
      valveActive = true;
      valveOpenDuration = openTimeMs;
      lastValveActionTime = now;
    }
  } else {
    fan->setFanSpeed(IDLE_SPEED);
    valve->closeValve();
  }
}


void SensorHandler::updateFromEncoder() {
  inMainMenu = inputManager->isMainMenu();
  if (inMainMenu) {
    const bool cwRotation = encoder->rotatedCW();
    const bool ccwRotation = encoder->rotatedCCW();

    if (cwRotation) {
      targetCo2 = std::min(targetCo2 + 5, static_cast<int>(MAX_CO2));
    }
    if (ccwRotation) {
      targetCo2 = std::max(targetCo2 - 5, static_cast<int>(MIN_CO2));
    }
  }
}


void SensorHandler::updateControl() {
  writeToEEPROM();
  copyValueFromEEPROM(EEPROM_CO2_CLOUD_ADDR, cloudtargetCo2);
  if (cloudtargetCo2 != 0 && cloudtargetCo2 != 0xFFFF) {
    targetCo2 = cloudtargetCo2;
    cloudtargetCo2 = 0;
    emptyValueFromEEPROM();
  }


  const float currentCo2 = gmpSensor->readMeasuredCO2();

  if (!std::isnan(currentCo2)) {
    handleValveAndFanLogic(currentCo2, targetCo2);
  } else {
  }
}

void SensorHandler::controlLoop() {
  while (true) {
    updateControl();
    updateFromEncoder();
    vTaskDelay(pdMS_TO_TICKS(60));
  }
}


void SensorHandler::controlTask(void *pvParameters) {
  auto *self = static_cast<SensorHandler *>(pvParameters);
  self->controlLoop();
}
