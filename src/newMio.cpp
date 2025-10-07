commit 3a0a39327245733101d898da9931eaccdeef0c88
Author: Nischal1313 <nischaga@metropolia.fi>
Date:   Tue Oct 7 10:11:30 2025 +0300

    BACKUP: Current state before rollback

diff --git a/src/produalMIO.cpp b/src/produalMIO.cpp
index 42e3b8c..cd211ef 100644
--- a/src/produalMIO.cpp
+++ b/src/produalMIO.cpp
@@ -1,3 +1,172 @@
+// #include "produalMIO.h"
+// #include <algorithm>
+// #include <cmath>
+// #include <cstdio>
+// #include "gmp252.h"
+//
+// ModbusMIO::ModbusMIO(std::shared_ptr<ModbusClient> modbus,
+//                      const SemaphoreHandle_t mutex)
+//     : modbus(std::move(modbus)),
+//       slaveAddress(1),
+//       busMutex(mutex) {}
+//
+// void ModbusMIO::controlLoop(const GMP252& sensor, const RotaryEncoder& rotaryEncoder) {
+//     const int co2Lvl = static_cast<int>(sensor.readMeasuredCO2());
+//     const int desiredValue = rotaryEncoder.currentRotationValue();
+//     handleValveLogic(co2Lvl, desiredValue);
+// }
+//
+// void ModbusMIO::handleValveLogic(const int co2Lvl,const int desiredCo2Lvl) {
+//     const int diff = desiredCo2Lvl - co2Lvl;
+//
+//     // Within ±20 ppm → idle
+//     if (std::abs(diff) <= 15) {
+//         setFanSpeed(0.0f);
+//         valve.closeValve();
+//         return;
+//     }
+//     if (diff > 0) {
+//         const int valveTimeMs = (diff >= 500) ? 1000
+//                         : (diff >= 250) ? 500
+//                         : (diff >= 100) ? 200
+//                         : (diff >= 50) ? 100
+//                         : 50;
+//
+//         valve.openValve();
+//         setFanSpeed(0.0f);
+//         vTaskDelay(pdMS_TO_TICKS(valveTimeMs));
+//         valve.closeValve();
+//     } else {
+//         setFanSpeed(100.0f);
+//         valve.closeValve();
+//         return;
+//     }
+//     // Idle fan after injection
+//     setFanSpeed(0.0f);
+// }
+//
+// bool ModbusMIO::setFanSpeed(float percent) const {
+//     MutexGuard lock(busMutex);
+//     if (!lock.owns_lock()) return false;
+//
+//     percent = std::clamp(percent, 0.0f, 100.0f);
+//     const auto value = static_cast<uint16_t>((percent / 100.0f) * 1000.0f);
+//
+//     modbus->set_destination_rtu_address(slaveAddress);
+//     nmbs_error err = modbus->write_single_register(REG_AO1, value);
+//
+//     return (err == NMBS_ERROR_NONE);
+// }
+//
+// bool ModbusMIO::isFanRunning() const {
+//     MutexGuard lock(busMutex);
+//     if (!lock.owns_lock()) return false;
+//
+//     modbus->set_destination_rtu_address(slaveAddress);
+//     return true; // placeholder
+// }
+//
+// float ModbusMIO::readFanSpeed() const {
+//     MutexGuard lock(busMutex);
+//     if (!lock.owns_lock()) return NAN;
+//
+//     modbus->set_destination_rtu_address(slaveAddress);
+//     uint16_t value{};
+//     nmbs_error err = modbus->read_holding_registers(REG_AO1, 1, &value);
+//     if (err != NMBS_ERROR_NONE) return NAN;
+//
+//     return (static_cast<float>(value) / 1000.0f) * 100.0f;
+// }
+// ============================================================================
+// produalMIO.cpp
+// ============================================================================
+// #include "produalMIO.h"
+// #include <algorithm>
+// #include <cmath>
+// #include <cstdio>
+// #include "gmp252.h"
+//
+// ModbusMIO::ModbusMIO(std::shared_ptr<ModbusClient> modbus,
+//                      const SemaphoreHandle_t mutex)
+//     : modbus(std::move(modbus)),
+//       slaveAddress(1),
+//       busMutex(mutex) {}
+//
+// void ModbusMIO::controlLoop(const GMP252& sensor, const RotaryEncoder& rotaryEncoder) {
+//     const int co2Lvl = static_cast<int>(sensor.readMeasuredCO2());
+//     const int desiredValue = rotaryEncoder.currentRotationValue();
+//     handleValveLogic(co2Lvl, desiredValue);
+// }
+//
+// void ModbusMIO::handleValveLogic(const int co2Lvl, const int desiredCo2Lvl) {
+//     const int diff = desiredCo2Lvl - co2Lvl;
+//
+//     // Within ±15 ppm → idle
+//     if (std::abs(diff) <= 15) {
+//         (void)setFanSpeed(0.0f);
+//         valve.closeValve();
+//         return;
+//     }
+//
+//     if (diff > 0) {
+//         // Need more CO2 - open valve
+//         const int valveTimeMs = (diff >= 500) ? 1000
+//                         : (diff >= 250) ? 500
+//                         : (diff >= 100) ? 200
+//                         : (diff >= 50) ? 100
+//                         : 50;
+//
+//         valve.openValve();
+//         (void)setFanSpeed(0.0f);
+//         vTaskDelay(pdMS_TO_TICKS(valveTimeMs));
+//         valve.closeValve();
+//     } else {
+//         // Too much CO2 - run fan to exhaust
+//         (void)setFanSpeed(100.0f);
+//         valve.closeValve();
+//         return;
+//     }
+//
+//     // Idle fan after injection
+//     (void)setFanSpeed(0.0f);
+// }
+//
+// bool ModbusMIO::setFanSpeed(float percent) const {
+//     MutexGuard lock(busMutex);
+//     if (!lock.owns_lock()) return false;
+//
+//     percent = std::clamp(percent, 0.0f, 100.0f);
+//     const auto value = static_cast<uint16_t>((percent / 100.0f) * 1000.0f);
+//
+//     modbus->set_destination_rtu_address(slaveAddress);
+//     nmbs_error err = modbus->write_single_register(REG_AO1, value);
+//
+//     return (err == NMBS_ERROR_NONE);
+// }
+//
+// bool ModbusMIO::isFanRunning() const {
+//     MutexGuard lock(busMutex);
+//     if (!lock.owns_lock()) return false;
+//
+//     modbus->set_destination_rtu_address(slaveAddress);
+//     return true; // placeholder
+// }
+//
+// float ModbusMIO::readFanSpeed() const {
+//     MutexGuard lock(busMutex);
+//     if (!lock.owns_lock()) return NAN;
+//
+//     modbus->set_destination_rtu_address(slaveAddress);
+//     uint16_t value{};
+//     nmbs_error err = modbus->read_holding_registers(REG_AO1, 1, &value);
+//     if (err != NMBS_ERROR_NONE) return NAN;
+//
+//     return (static_cast<float>(value) / 1000.0f) * 100.0f;
+// }
+
+// ============================================================================
+// produalMIO.cpp
+// ============================================================================
 #include "produalMIO.h"
 #include <algorithm>
 #include <cmath>
@@ -7,6 +176,7 @@
 ModbusMIO::ModbusMIO(std::shared_ptr<ModbusClient> modbus,
                      const SemaphoreHandle_t mutex)
     : modbus(std::move(modbus)),
+      valve(),
       slaveAddress(1),
       busMutex(mutex) {}
 
@@ -16,7 +186,7 @@ void ModbusMIO::controlLoop(const GMP252& sensor, const RotaryEncoder& rotaryEnc
     handleValveLogic(co2Lvl, desiredValue);
 }
 
-void ModbusMIO::handleValveLogic(const int co2Lvl,const int desiredCo2Lvl) {
+void ModbusMIO::handleValveLogic(const int co2Lvl, const int desiredCo2Lvl) {
     const int diff = desiredCo2Lvl - co2Lvl;
 
     // Within ±20 ppm → idle
@@ -76,4 +246,4 @@ float ModbusMIO::readFanSpeed() const {
     if (err != NMBS_ERROR_NONE) return NAN;
 
     return (static_cast<float>(value) / 1000.0f) * 100.0f;
-}
+}
\ No newline at end of file
