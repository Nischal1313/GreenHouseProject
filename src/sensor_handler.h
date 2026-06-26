#pragma once
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
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "eeprom/eeprom.h"
#include "inputManager.h"

struct SensorValues
{
    float temperatureM{0.0f};
    float humidityM{0.0f};
    float co2M{0.0f};
    float fanSpeedM{0.0f};
    bool valveOpenM{false};
    int targetCo2M{};
};

class SensorHandler
{
public:
    SensorHandler(SemaphoreHandle_t mutexP
                  , std::shared_ptr<RotaryEncoder> const &pEncoderPtrP
                  , SemaphoreHandle_t eepromMutexP
                  , Eeprom &rEepromP
                  , std::shared_ptr<InputManager> const &pInputManagerP);

    SensorValues getReadings() const;

    void copyValueFromEEPROM(uint16_t addrP, int &rValueToWriteToP) const;

    void emptyValueFromEEPROM() const;

    void writeToEEPROM() const;

    void updateControl();

    /// @brief Direct valve/fan control (called by state machine)
    void handleValveAndFanLogic(float co2LvlP, int desiredCo2LvlP) const;

    /// @brief Update setpoint from encoder (called by state machine)
    void updateFromEncoder();

    [[noreturn]] void controlLoop();

    static void controlTask(void *pvParameters);

    static constexpr uint16_t EEPROM_CO2_ADDR = 0x10;
    static constexpr uint16_t EEPROM_CO2_CLOUD_ADDR = 0x0200;

private:
    std::shared_ptr<GMP252> pGmpSensorM;
    std::shared_ptr<HMP60> pHmpSensorM;
    std::shared_ptr<ModbusMIO> pFanM;
    std::shared_ptr<Valve> pValveM;
    std::shared_ptr<RotaryEncoder> pEncoderM;
    std::shared_ptr<InputManager> pInputManagerM;
    Eeprom *pEepromM;

    SemaphoreHandle_t mutexM;
    SemaphoreHandle_t eepromMutexM;

    static constexpr uint ACCEPTED_RANGE = 10;
    static constexpr uint FULL_SPEED = 100;
    static constexpr uint IDLE_SPEED = 0;
    static constexpr uint VALVE_IDLE_TIME = 30000; // ms
    static constexpr uint MAX_VALVE_OPEN_TIME = 2000; // ms
    static constexpr uint CONTROL_LOOP_DELAY = 300; // ms
    static constexpr uint16_t MIN_CO2 = 200;
    static constexpr uint16_t MAX_CO2 = 1500;

    int targetCo2M{};
    int cloudTargetCo2M{0};
    mutable uint32_t lastValveActionTimeM{0};
    mutable bool valveActiveM{false};
    mutable uint32_t valveOpenDurationM{0};
    bool inMainMenuM;
};
