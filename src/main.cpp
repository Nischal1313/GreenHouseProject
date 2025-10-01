//if the co2 value is over 2000 full blast fan.
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
#include "event_groups.h"
#include "ModbusClient.h"

#include "ssd1306os.h"
#include "gmp252.h"
#include "hmp60.h"
#include "produalMIO.h"
#include "relayController.h"

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
// Event group and debug queue
EventGroupHandle_t eventGroup;

constexpr TickType_t WATCHDOG_TIMER = pdMS_TO_TICKS(30000);

constexpr int TASK_HIGH_PRIORITY = 2 + tskIDLE_PRIORITY;
constexpr int WATCHDOG_PRIORITY = 2 + tskIDLE_PRIORITY;
constexpr int TASK_LOW_PRIORITY = 1 + tskIDLE_PRIORITY;

constexpr EventBits_t BIT_TASK_FAN = (1 << 0);
constexpr EventBits_t BIT_TASK_GMP = (1 << 1);
constexpr EventBits_t BIT_TASK_HMP = (1 << 2);
constexpr EventBits_t ALL_TASK_BITS = BIT_TASK_FAN | BIT_TASK_GMP | BIT_TASK_HMP;


void init_function() {
    eventGroup = xEventGroupCreate();
}


[[noreturn]] void modbusGmpTask(void *pvParameters) {
    const auto debug = static_cast<Debug *>(pvParameters);
    const GMP252 gmpSensor(modbus, modbusMutex);
    while (true) {
        float co2 = gmpSensor.readMeasuredCO2();
        char buf[64];
        if (!std::isnan(co2)) {
            snprintf(buf, sizeof(buf), "CO2: %.1f ppm\n", co2);
            debug->print(buf);
        } else {
            snprintf(buf, sizeof(buf), "Sensor error\n");
            debug->print(buf);
        }

        display.text(buf, 2, 10);
        display.show();
        xEventGroupSetBits(eventGroup, BIT_TASK_GMP);
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

[[noreturn]] void modbusHmpTask(void *pvParameters) {
    // Initialize sensor with modbus + mutex
    const auto debug = static_cast<Debug *>(pvParameters);
    const HMP60 hmpSensor(modbus, modbusMutex);
    while (true) {
        const float hum = hmpSensor.readHumidity();
        const float temp = hmpSensor.readTemperature();
        char buf[64];
        if (!std::isnan(hum) && !std::isnan(temp)) {
            snprintf(buf, sizeof(buf), "%.1f |,%.1f \n", hum, temp);
            debug->print(buf);
        } else {
            snprintf(buf, sizeof(buf), "Sensor error\n");
            debug->print(buf);
        }
        display.text(buf, 2, 20);
        display.show();
        xEventGroupSetBits(eventGroup, BIT_TASK_HMP);
        vTaskDelay(pdMS_TO_TICKS(8000));
    }
}


[[noreturn]] void modbusFanTask(void *pvParameters) {
    const ModbusMIO modbusFan(modbus, modbusMutex);
    const auto debug = static_cast<Debug *>(pvParameters);
    constexpr TickType_t taskDelay = pdMS_TO_TICKS(9000);
    while (true) {
        constexpr float desiredFanSpeed = 0.0f;
        const bool success = modbusFan.setFanSpeed(desiredFanSpeed);
        const float speed = modbusFan.readFanSpeed();

        char buf[128];
        if (success) {
            snprintf(buf, sizeof(buf), "Fan speed set to %.1f%%, fan is running\n", desiredFanSpeed);
            debug->print(buf);
        }
        debug->print(buf);
        snprintf(buf, sizeof(buf), "Speed %.1f%%", speed);
        debug->print(buf);
        display.text(buf, 2, 30);
        xEventGroupSetBits(eventGroup, BIT_TASK_FAN);
        vTaskDelay(taskDelay);
    }
}

[[noreturn]] void relayTask(void *pvParameters) {
    RELAYCONTROL valve(9);
    while (true) {
        valve.taskStep();
        xEventGroupSetBits(eventGroup, BIT_TASK_FAN);
        vTaskDelay(100);
    }
}


[[noreturn]] void watchDogTimer(void *pvParameters) {
    const auto debug = static_cast<Debug *>(pvParameters);
    TickType_t lastOK = xTaskGetTickCount();
    while (true) {
        const EventBits_t result = xEventGroupWaitBits(eventGroup, ALL_TASK_BITS,
            pdTRUE, pdTRUE, WATCHDOG_TIMER);

        char buf[128];
        char wdStatus[32];
        snprintf(wdStatus, sizeof(wdStatus), "W.D.[%c,%c,%c]",
                 (result & BIT_TASK_FAN) ? '1' : '#',
                 (result & BIT_TASK_GMP) ? '2' : '#',
                 (result & BIT_TASK_HMP) ? '3' : '#');

        if ((result & ALL_TASK_BITS) == ALL_TASK_BITS) {
            TickType_t now = xTaskGetTickCount();
            snprintf(buf, sizeof(buf), "Watchdog: OK, %lu ms since last OK\n",
                     static_cast<unsigned long>((now - lastOK) * portTICK_PERIOD_MS));
            debug->print(buf);
            lastOK = now;
        } else {
            debug->print("Watchdog FAIL! Missing tasks: ");
            if (!(result & BIT_TASK_FAN)) debug->print("Fan ");
            if (!(result & BIT_TASK_GMP)) debug->print("CO2 ");
            if (!(result & BIT_TASK_HMP)) debug->print("HMP ");
            debug->print("\n");
        }

        display.text(wdStatus, 2, 40);
        display.show();

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}


int main() {
    stdio_init_all();
    init_function();
    auto debug{std::make_shared<Debug>()};
    auto debugTask{std::make_unique<DebugTask>(debug)};
    debug->print("Program started.\n");

    xTaskCreate(watchDogTimer, "WatchDogTimer",
                1024, debug.get(),
                WATCHDOG_PRIORITY, nullptr);

    xTaskCreate(modbusGmpTask, "SSD1306", 512, debug.get(),
                tskIDLE_PRIORITY + 1, nullptr);

    xTaskCreate(modbusFanTask, "nn", 512, debug.get(),
                tskIDLE_PRIORITY + 1, nullptr);

    xTaskCreate(modbusHmpTask, "HMP", 512, debug.get(),
                1, nullptr);

    xTaskCreate(relayTask, "Relay Task",
                1024, debug.get(),
                1, nullptr);

    vTaskStartScheduler();

    while (true) {
    };
}
