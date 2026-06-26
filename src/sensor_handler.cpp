#include "sensor_handler.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "FreeRTOS.h"
#include "task.h"
#include "eeprom/eeprom.h"

SensorHandler::SensorHandler(SemaphoreHandle_t const mutexP
                             , std::shared_ptr<RotaryEncoder> const &pEncoderPtrP
                             , SemaphoreHandle_t const eepromMutexP
                             , Eeprom &rEepromP
                             , std::shared_ptr<InputManager> const &pInputManagerP
)
    : pEncoderM{pEncoderPtrP}
    , pInputManagerM{pInputManagerP}
    , pEepromM{&rEepromP}
    , mutexM{mutexP}
    , eepromMutexM{eepromMutexP}
{
    auto uart = std::make_shared<PicoOsUart>(1, 4, 5, 9600, 8, 256, 256);

    auto modbusClient = std::make_shared<ModbusClient>(uart);

    pGmpSensorM = std::make_shared<GMP252>(modbusClient, mutexM);
    pHmpSensorM = std::make_shared<HMP60>(modbusClient, mutexM);
    pFanM = std::make_shared<ModbusMIO>(modbusClient, mutexM);
    pValveM = std::make_shared<Valve>();
    copyValueFromEEPROM(EEPROM_CO2_ADDR, targetCo2M);
    inMainMenuM = true;
}

SensorValues SensorHandler::getReadings() const
{
    SensorValues result{};
    result.co2M = pGmpSensorM->readMeasuredCO2();
    result.temperatureM = pHmpSensorM->readTemperature();
    result.humidityM = pHmpSensorM->readHumidity();
    result.fanSpeedM = pFanM->readFanSpeed();
    result.valveOpenM = pValveM->valveStatus();
    result.targetCo2M = targetCo2M;
    return result;
}

void SensorHandler::copyValueFromEEPROM(uint16_t const addrP,
                                        int &rValueToWriteToP) const
{
    MutexGuard const lock{eepromMutexM};
    uint8_t buf[2] = {0};
    if (pEepromM->readBlock(addrP, buf, 2))
    {
        int const checkingValue = (buf[0] << 8) | buf[1];
        if (checkingValue >= 200 && checkingValue <= 1500)
        {
            rValueToWriteToP = checkingValue;
        }
    }
}

void SensorHandler::emptyValueFromEEPROM() const
{
    constexpr uint8_t buf[2] = {
        static_cast<uint8_t>(0 >> 8),
        static_cast<uint8_t>(0 & 0xFF)
    };
    pEepromM->writeBlock(EEPROM_CO2_CLOUD_ADDR, buf, 2);
}

void SensorHandler::writeToEEPROM() const
{
    static absolute_time_t lastWriteTimeS{0};
    int64_t const now = to_ms_since_boot(get_absolute_time());
    int64_t const elapsed = now - to_ms_since_boot(lastWriteTimeS);

    if (elapsed >= 15000)
    {
        MutexGuard const lock{eepromMutexM};
        uint8_t const buf[2] = {
            static_cast<uint8_t>(targetCo2M >> 8),
            static_cast<uint8_t>(targetCo2M & 0xFF)
        };
        if (pEepromM->writeBlock(EEPROM_CO2_ADDR, buf, 2))
        {
            lastWriteTimeS = get_absolute_time();
        }
    }
}

void SensorHandler::handleValveAndFanLogic(float const co2LvlP,
                                           int const desiredCo2LvlP) const
{
    int const diff = desiredCo2LvlP - static_cast<int>(co2LvlP);
    uint32_t const now = to_ms_since_boot(get_absolute_time());

    if (valveActiveM)
    {
        if (now - lastValveActionTimeM >= valveOpenDurationM)
        {
            pValveM->closeValve();
            pFanM->setFanSpeed(IDLE_SPEED);
            valveActiveM = false;
            lastValveActionTimeM = now;
        }
    }
    else if (now - lastValveActionTimeM >= VALVE_IDLE_TIME)
    {
        if (std::abs(diff) > ACCEPTED_RANGE)
        {
            if (diff < 0)
            {
                pFanM->setFanSpeed(FULL_SPEED);
                pValveM->closeValve();
            }
            else
            {
                int openTimeMs = (diff * static_cast<int>(MAX_VALVE_OPEN_TIME))
                                 / 1000;
                openTimeMs = std::clamp(openTimeMs, 50,
                                        static_cast<int>(MAX_VALVE_OPEN_TIME));
                pValveM->openValve();
                pFanM->setFanSpeed(IDLE_SPEED);
                valveActiveM = true;
                valveOpenDurationM = openTimeMs;
                lastValveActionTimeM = now;
            }
        }
        else
        {
            pFanM->setFanSpeed(IDLE_SPEED);
            pValveM->closeValve();
        }
    }
}

void SensorHandler::updateFromEncoder()
{
    inMainMenuM = pInputManagerM->isMainMenu();
    if (inMainMenuM)
    {
        bool const cwRotation = pEncoderM->rotatedCW();
        bool const ccwRotation = pEncoderM->rotatedCCW();

        if (cwRotation)
        {
            targetCo2M = std::min(targetCo2M + 5, static_cast<int>(MAX_CO2));
        }
        if (ccwRotation)
        {
            targetCo2M = std::max(targetCo2M - 5, static_cast<int>(MIN_CO2));
        }
    }
}

void SensorHandler::updateControl()
{
    writeToEEPROM();
    copyValueFromEEPROM(EEPROM_CO2_CLOUD_ADDR, cloudTargetCo2M);
    if (cloudTargetCo2M != 0 && cloudTargetCo2M != 0xFFFF)
    {
        targetCo2M = cloudTargetCo2M;
        cloudTargetCo2M = 0;
        emptyValueFromEEPROM();
    }

    float const currentCo2 = pGmpSensorM->readMeasuredCO2();

    if (!std::isnan(currentCo2))
    {
        handleValveAndFanLogic(currentCo2, targetCo2M);
    }
}

void SensorHandler::controlLoop()
{
    while (true)
    {
        updateControl();
        updateFromEncoder();
        vTaskDelay(pdMS_TO_TICKS(60));
    }
}

void SensorHandler::controlTask(void *pvParameters)
{
    auto *self = static_cast<SensorHandler *>(pvParameters);
    self->controlLoop();
}
