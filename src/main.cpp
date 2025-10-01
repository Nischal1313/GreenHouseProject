// #include <cmath>
// #include <cstdio>
// #include <iostream>
// #include <memory>
// #include "FreeRTOS.h"
// #include "event_groups.h"
// #include "gmp252.h"
// #include "pico/stdlib.h"
// #include "task.h"
// #include "hardware/gpio.h"
// #include "uart/PicoOsUart.h"
// #include "modbus/ModbusClient.h"
// #include "semphr.h"
// #include "produalMIO.h"
// #include "mutexGuard.h"
// #include "hmp60.h"
// #include "sdp610.h"
// #include "IPStack.h"
// #include "debug.h"
// #include "queue.h"
// #include "relayController.h"
// #include "displayMenu.h"
//
// extern "C" {
// uint32_t read_runtime_ctr(void) {
//     return time_us_32();
// }}
//
// constexpr int TASK_HIGH_PRIORITY = 2 + tskIDLE_PRIORITY;
// constexpr int WATCHDOG_PRIORITY = 2 + tskIDLE_PRIORITY;
// constexpr int TASK_LOW_PRIORITY = 1 + tskIDLE_PRIORITY;
//
// constexpr EventBits_t BIT_TASK_FAN = (1 << 0);
// constexpr EventBits_t BIT_TASK_GMP = (1 << 1);
// constexpr EventBits_t BIT_TASK_HMP = (1 << 2);
// constexpr EventBits_t BIT_TASK_SDP = (1 << 3);
// constexpr EventBits_t ALL_TASK_BITS = BIT_TASK_FAN | BIT_TASK_GMP | BIT_TASK_HMP | BIT_TASK_SDP;
//
// i2c_inst_t *i2c_instance = i2c1;
// constexpr uint SDA_PIN = 14;
// constexpr uint SCL_PIN = 15;
//
// SemaphoreHandle_t modbusMutex;
// SemaphoreHandle_t i2cMutex;
// SemaphoreHandle_t buttonMutex;
//
// std::shared_ptr<PicoOsUart> uart;
// std::shared_ptr<ModbusClient> modbus;
//
//
//
// EventGroupHandle_t eventGroup;
//
// constexpr TickType_t WATCHDOG_TIMER = pdMS_TO_TICKS(30000);
//
//
// void initFunction() {
//     stdio_init_all();
//     // Create mutexes
//     modbusMutex = xSemaphoreCreateMutex();
//     i2cMutex = xSemaphoreCreateMutex();
//     buttonMutex = xSemaphoreCreateMutex();
//     // Event group and debug queue
//     eventGroup = xEventGroupCreate();
//     // UART hardware init
//     uart_init(uart1, 9600);
//     gpio_set_function(4, GPIO_FUNC_UART);
//     gpio_set_function(5, GPIO_FUNC_UART);
//
//     uart_set_format(uart1, 8, 1, UART_PARITY_EVEN);
//
//     // Uart and Modbus client
//     uart = std::make_shared<PicoOsUart>(1, 4, 5, 9600, 8, 256, 256);
//     modbus = std::make_shared<ModbusClient>(uart);
//     // Initialize I2C1 for SDP610
//     i2c_init(i2c_instance, 100000);
//     gpio_set_function(SDA_PIN, GPIO_FUNC_I2C);
//     gpio_set_function(SCL_PIN, GPIO_FUNC_I2C);
//     gpio_pull_up(SDA_PIN);
//     gpio_pull_up(SCL_PIN);
// }
//
//
// // Tasks
// [[noreturn]] void watchDogTimer(void *pvParameters) {
//     const auto debug = static_cast<Debug *>(pvParameters);
//     TickType_t lastOK = xTaskGetTickCount();
//     while (true) {
//         EventBits_t result = xEventGroupWaitBits(eventGroup, ALL_TASK_BITS, pdTRUE, pdTRUE, WATCHDOG_TIMER);
//         if ((result & ALL_TASK_BITS) == ALL_TASK_BITS) {
//             TickType_t now = xTaskGetTickCount();
//             char buf[128];
//             snprintf(buf, sizeof(buf), "Watchdog: OK, %lu ms since last OK\n",
//                      static_cast<unsigned long>((now - lastOK) * portTICK_PERIOD_MS));
//             debug->print(buf);
//             lastOK = now;
//         } else {
//             EventBits_t missingBits = ALL_TASK_BITS & ~result;
//             debug->print("Watchdog FAIL! Missing tasks: -> ");
//             if (missingBits & BIT_TASK_FAN) debug->print("  Fan control task\n");
//             if (missingBits & BIT_TASK_GMP) debug->print("  GMP252 CO2 sensor task\n");
//             if (missingBits & BIT_TASK_HMP) debug->print("  HMP60 humidity/temp task\n");
//             if (missingBits & BIT_TASK_SDP) debug->print("  SDP610 pressure sensor task\n");
//             vTaskDelay(pdMS_TO_TICKS(1000));
//         }
//     }
// }
//
// [[noreturn]] void modbusFanTask(void *pvParameters) {
//     const ModbusMIO modbusFan(modbus, modbusMutex);
//     const auto debug = static_cast<Debug *>(pvParameters);
//     constexpr TickType_t taskDelay = pdMS_TO_TICKS(60000);
//     while (true) {
//         constexpr float desiredFanSpeed = 0.0f;
//         bool success = modbusFan.setFanSpeed(desiredFanSpeed);
//         bool fanRunning = modbusFan.isFanRunning();
//
//         char buf[128];
//         if (success) {
//             snprintf(buf, sizeof(buf), "Fan speed set to %.1f%%, fan is running\n", desiredFanSpeed);
//             debug->print(buf);
//         }
//         if (fanRunning) {
//             snprintf(buf, sizeof(buf), "Fan is running.\n");
//             debug->print(buf);
//         } else {
//             snprintf(buf, sizeof(buf), "Failed to set speed.\n");
//             debug->print(buf);
//         }
//
//         xEventGroupSetBits(eventGroup, BIT_TASK_FAN);
//         vTaskDelay(taskDelay);
//     }
// }
//
// [[noreturn]] void modbusGmpTask(void *pvParameters) {
//     const auto debug = static_cast<Debug *>(pvParameters);
//     const GMP252 gmpSensor(modbus, modbusMutex);
//     constexpr TickType_t taskDelay = pdMS_TO_TICKS(3000);
//     while (true) {
//         const float co2 = gmpSensor.readMeasuredCO2();
//         if (!std::isnan(co2)) {
//             char buf[128];
//             snprintf(buf, sizeof(buf),
//                      "GMP252 - CO2: %.1f ppm\n", co2);
//             debug->print(buf);
//         } else debug->print("GMP252 sensor read failed!\n");
//
//         xEventGroupSetBits(eventGroup, BIT_TASK_GMP);
//         vTaskDelay(taskDelay);
//     }
// }
//
// [[noreturn]] void modbusHmpTask(void *pvParameters) {
//     const auto debug = static_cast<Debug *>(pvParameters);
//     const HMP60 hmpSensor(modbus, modbusMutex);
//     constexpr TickType_t taskDelay = pdMS_TO_TICKS(4000);
//     while (true) {
//         const float hum = hmpSensor.readHumidity();
//         const float temp = hmpSensor.readTemperature();
//
//         if (!std::isnan(hum) && !std::isnan(temp)) {
//             char buf[128];
//             snprintf(buf, sizeof(buf), "HMP60 - Humidity: %.1f%%, Temperature: %.1fC\n", hum, temp);
//             debug->print(buf);
//         } else debug->print("HMP60 sensor read failed!\n");
//
//         xEventGroupSetBits(eventGroup, BIT_TASK_HMP);
//         vTaskDelay(taskDelay);
//     }
// }
//
// [[noreturn]] void I2cPressureSensorTask(void *pvParameters) {
//     const auto debug = static_cast<Debug *>(pvParameters);
//     SDP610 pressureSensor(i2c1, SDA_PIN, SCL_PIN, i2cMutex);
//     pressureSensor.init();
//     constexpr TickType_t taskDelay = pdMS_TO_TICKS(5000);
//
//     while (true) {
//         float pressure = pressureSensor.readPressurePa();
//         if (!std::isnan(pressure)) {
//             char buf[128];
//             snprintf(buf, sizeof(buf), "SDP610 - Pressure: %.2f Pa\n", pressure);
//             debug->print(buf);
//         } else {
//             debug->print("SDP610 pressure sensor read failed!\n");
//         }
//
//         xEventGroupSetBits(eventGroup, BIT_TASK_SDP);
//         vTaskDelay(taskDelay);
//     }
// }
//
// [[noreturn]] void relayTask(void *pvParameters) {
//     RELAYCONTROL valve(9);
//     while (true) {
//         valve.taskStep();
//         vTaskDelay(100);
//     }
// }
//
//
// [[noreturn]] int main() {
//     // REMEMBER TO TURN ON THE DRT MODE IN THE DEBUGGER TO SEE ANY DEBUG.
//     initFunction();
//     auto debug{std::make_shared<Debug>()};
//     auto debugTask{std::make_unique<DebugTask>(debug)};
//     debug->print("Program started.\n");
//     //
//     // Create OLED
//     auto picoI2C = std::make_shared<PicoI2C>(1);
//     auto oled = std::make_shared<ssd1306os>(picoI2C);
//
//     // Display system + task
//     // auto displaySystem = std::make_shared<DisplaySystem>(oled, i2cMutex);
//     // auto displayTask = std::make_shared<DisplayTask>(displaySystem);
//
//
//
//     xTaskCreate(relayTask, "Relay Task",
//         1024, debug.get(),
//         TASK_HIGH_PRIORITY, nullptr);
//
//     xTaskCreate(watchDogTimer, "WatchDogTimer",
//         1024, debug.get(),
//         WATCHDOG_PRIORITY, nullptr);
//
//     xTaskCreate(modbusFanTask, "ModbusFanTask",
//         1024, debug.get(),
//         TASK_HIGH_PRIORITY, nullptr);
//
//     xTaskCreate(modbusGmpTask, "ModbusGmpTask",
//         1024, debug.get(),
//         TASK_HIGH_PRIORITY, nullptr);
//
//     xTaskCreate(modbusHmpTask, "ModbusHmpTask",
//         1024, debug.get(),
//         TASK_HIGH_PRIORITY, nullptr);
//
//     xTaskCreate(I2cPressureSensorTask,
//         "PressureSensorTask",
//         1024, debug.get(),
//         TASK_HIGH_PRIORITY, nullptr);
//
//     debug->print("All tasks created. Starting scheduler.\n");
//
//     vTaskStartScheduler();
//     while (true);
// }
//if the co2 value is over 2000 full blast.
// main.cpp
// FreeRTOS + Modbus + OLED system with mutex protection for sensors
// OLED (ssd1306os) does not need external mutex because PicoI2C uses Fmutex internally.


#include <cmath>
#include <iostream>
#include <sstream>
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "hardware/gpio.h"
#include "PicoOsUart.h"

#include "hardware/timer.h"
#include "pico/stdio.h"

#include <cstdio>

#include "debug.h"
#include "ModbusClient.h"

#include "ssd1306os.h"
#include "gmp252.h"
#include "hmp60.h"

extern "C" {
uint32_t read_runtime_ctr(void) {
    return time_us_32();
}
}

const SemaphoreHandle_t modbusMutex = xSemaphoreCreateMutex();


auto uart = std::make_shared<PicoOsUart>(1, 4, 5, 9600, 8, 256, 256); // tx=4, rx=5
auto modbus = std::make_shared<ModbusClient>(uart);
auto i2cbus = std::make_shared<PicoI2C>(1, 400000);
ssd1306os display(i2cbus);

void modbusGmpTask(void *pvParameters) {
    // Initialize display


    GMP252 gmpSensor(modbus, modbusMutex);

    while (true) {
        float co2 = gmpSensor.readMeasuredCO2();
        char buf[64];
        if (!std::isnan(co2)) {
            snprintf(buf, sizeof(buf), "CO2: %.1f ppm", co2);
        } else {
            snprintf(buf, sizeof(buf), "Sensor error");
        }

        display.text(buf, 10, 10);
        display.show();

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

[[noreturn]] void modbusHmpTask(void *pvParameters) {
    // Create a mutex local to this task
    // Initialize UART for Modbus RTU
    // Debug interface passed via pvParameters
    const auto debug = static_cast<Debug *>(pvParameters);

    // Initialize sensor with modbus + mutex
    const HMP60 hmpSensor(modbus, modbusMutex);

    constexpr TickType_t taskDelay = pdMS_TO_TICKS(4000);
    while (true) {
        const float hum = hmpSensor.readHumidity();
        const float temp = hmpSensor.readTemperature();
        char buf[64];
        if (!std::isnan(hum) && !std::isnan(temp)) {
            snprintf(buf, sizeof(buf), "%.1f |,%.1f ", hum, temp);
        } else {
            snprintf(buf, sizeof(buf), "Sensor error");
        }
        display.text(buf, 20, 3);
        display.show();

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}


int main() {
    stdio_init_all();
    printf("\nBoot\n");
    display.fill(0);
    xTaskCreate(modbusGmpTask, "SSD1306", 512, (void *) nullptr,
                tskIDLE_PRIORITY + 1, nullptr);

    xTaskCreate(modbusHmpTask, "nn", 512, (void *) nullptr,
                tskIDLE_PRIORITY + 1, nullptr);
    vTaskStartScheduler();

    while (true) {
    };
}
