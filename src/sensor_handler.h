#pragma once
#include <chrono>
#include "sensors/gmp252.h"
#include "sensors/hmp60.h"
#include "sensors/produalMIO.h"
#include "sensors/relayController.h"
#include "setpoint_manager.h"
#include "mutexGuard.h"
#include "uart/PicoOsUart.h"
#include "modbus/ModbusClient.h"
#include <memory>
// #include <vector>
#include <functional>
#include "rotary_encoder.h"

#include <algorithm>  // for std::min, std::max
#include <cmath>      // for std::isnan
#include <cstdio>
#include "sensor_handler.h"
#include "rotary_encoder.h"


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
                , const std::shared_ptr<RotaryEncoder> &);

  // Read all sensors and update cached values
  SensorValues getReadings() const;

  // Control valve/fan based on current CO2 vs setpoint
  void updateControl() const;

  // Get latest cached sensor readings

  // Main control loop for FreeRTOS task
  [[noreturn]] void controlLoop();

  // FreeRTOS task entry point
  static void controlTask(void *pvParameters);

private:
  // Hardware interfaces
  std::shared_ptr<GMP252> gmpSensor;
  std::shared_ptr<HMP60> hmpSensor;
  std::shared_ptr<ModbusMIO> fan;
  std::shared_ptr<VALVE> valve;

  // Thread safety
  SemaphoreHandle_t mutex;

  // Cached sensor data
  // SensorValues latestReadings;

  std::shared_ptr<RotaryEncoder> encoder; // Changed to shared_ptr - CRITICAL FIX

  // Control constants
  static constexpr uint ACCEPTED_RANGE = 10;
  static constexpr uint FULL_SPEED = 100;
  static constexpr uint IDLE_SPEED = 0;
  static constexpr uint VALVE_IDLE_TIME = 5'000; // ms
  static constexpr uint MAX_VALVE_OPEN_TIME = 2'000; // ms
  static constexpr uint CONTROL_LOOP_DELAY = 300; // ms
  static constexpr uint16_t MIN_CO2 = 200;
  static constexpr uint16_t MAX_CO2 = 1500;

  int targetCo2{};

  // Control logic
  void handleValveAndFanLogic(float co2Lvl, int desiredCo2Lvl) const;

  void updateFromEncoder();
};
