#pragma once
#include <chrono>
#include "sensors/gmp252.h"
#include "sensors/hmp60.h"
#include "sensors/produalMIO.h"
#include "sensors/relayController.h"
#include "mutexGuard.h"
#include "uart/PicoOsUart.h"
#include "modbus/ModbusClient.h"
#include <memory>
#include <vector>
#include <functional>
#include "rotary_encoder.h"

#include <algorithm>  // for std::min, std::max
#include <cmath>      // for std::isnan
#include <cstdio>
#include "sensor_handler.h"
#include "rotary_encoder.h"
#include "eeprom/eeprom.h"
#include "inputManager.h"


struct SensorValues {
  float temperature{0.0f};
  float humidity{0.0f};
  float co2{0.0f};
  float fanSpeed{0.0f};
  bool valveOpen{false};
  int targetCo2{};
};

class SensorHandler {
public:
  SensorHandler(SemaphoreHandle_t mutex
                , const std::shared_ptr<RotaryEncoder> &,
                SemaphoreHandle_t eepromMutex
                , Eeprom &eeprom, const std::shared_ptr<InputManager> &inputManager);


  SensorValues getReadings() const;

  void copyValueFromEEPROM(uint16_t addr, int &valueToWriteTo) const;

  void emptyValueFromEEPROM() const;

  void writeToEEPROM() const;


  void updateControl();


  [[noreturn]] void controlLoop();

  static void controlTask(void *pvParameters);

  static constexpr uint16_t EEPROM_CO2_ADDR = 0x10;
  static constexpr uint16_t EEPROM_CO2_CLOUD_ADDR = 0x0200;

private:
  std::shared_ptr<GMP252> gmpSensor;
  std::shared_ptr<HMP60> hmpSensor;
  std::shared_ptr<ModbusMIO> fan;
  std::shared_ptr<VALVE> valve;
  std::shared_ptr<RotaryEncoder> encoder;
  std::shared_ptr<InputManager> inputManager;
  Eeprom *eeprom;

  SemaphoreHandle_t mutex;
  SemaphoreHandle_t eepromMutex;

  static constexpr uint ACCEPTED_RANGE = 10;
  static constexpr uint FULL_SPEED = 100;
  static constexpr uint IDLE_SPEED = 0;
  static constexpr uint VALVE_IDLE_TIME = 30'000; // ms
  static constexpr uint MAX_VALVE_OPEN_TIME = 2'000; // ms
  static constexpr uint CONTROL_LOOP_DELAY = 300; // ms
  static constexpr uint16_t MIN_CO2 = 200;
  static constexpr uint16_t MAX_CO2 = 1500;


  int targetCo2{};
  int cloudtargetCo2 = 0;
  mutable uint32_t lastValveActionTime{0};
  mutable bool valveActive{false};
  mutable uint32_t valveOpenDuration{0};
  bool inMainMenu;

  void handleValveAndFanLogic(float co2Lvl, int desiredCo2Lvl) const;

  void updateFromEncoder();
};
