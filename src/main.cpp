#include <cmath>
#include <cstdio>
#include <memory>
#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"
#include "hardware/gpio.h"
#include "pico/stdio.h"
#include "PicoOsUart.h"

#include "ssd1306os.h"
#include "gmp252.h"
#include "hmp60.h"
#include "produalMIO.h"
#include "displayMenu.h"
#include "debug.h"

// Globals
EventGroupHandle_t eventGroup;
constexpr EventBits_t BIT_TASK_FAN = (1 << 0);
constexpr EventBits_t BIT_TASK_GMP = (1 << 1);
constexpr EventBits_t BIT_TASK_HMP = (1 << 2);
constexpr EventBits_t ALL_TASK_BITS = BIT_TASK_FAN | BIT_TASK_GMP | BIT_TASK_HMP;

extern "C" {
uint32_t read_runtime_ctr(void) {
    return time_us_32();
}
}

const SemaphoreHandle_t modbusMutex = xSemaphoreCreateMutex();
auto uart = std::make_shared<PicoOsUart>(1, 4, 5, 9600, 8, 256, 256);
auto modbus = std::make_shared<ModbusClient>(uart);
auto i2cbus = std::make_shared<PicoI2C>(1, 400000);
auto oledShared = std::make_shared<ssd1306os>(i2cbus);
HANDLEC02INPUT co2Input;
// EncoderHandler encoderHandler(ENCODER_PIN_A, ENCODER_PIN_B);
GMP252 gmp252(modbus, modbusMutex);

void init_function() {
    eventGroup = xEventGroupCreate();
}

// --- Task Implementations ---

[[noreturn]] void modbusGmpTask(void *pvParameters) {
    auto *debug = static_cast<Debug *>(pvParameters);

    while (true) {
        float co2 = gmp252.readMeasuredCO2();
        char buf[64];
        if (!std::isnan(co2)) {
            snprintf(buf, sizeof(buf), "CO2: %.1f ppm\n", co2);
            debug->print(buf);
        } else {
            snprintf(buf, sizeof(buf), "Sensor error\n");
            debug->print(buf);
        }

        oledShared->text(buf, 2, 10);
        oledShared->show();
        xEventGroupSetBits(eventGroup, BIT_TASK_GMP);
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

[[noreturn]] void modbusHmpTask(void *pvParameters) {
    auto *debug = static_cast<Debug *>(pvParameters);
    HMP60 hmpSensor(modbus, modbusMutex);

    while (true) {
        float hum = hmpSensor.readHumidity();
        float temp = hmpSensor.readTemperature();
        char buf[64];

        if (!std::isnan(hum) && !std::isnan(temp)) {
            snprintf(buf, sizeof(buf), "%.1f%% | %.1f°C\n", hum, temp);
            debug->print(buf);
        } else {
            snprintf(buf, sizeof(buf), "Sensor error\n");
            debug->print(buf);
        }

        oledShared->text(buf, 2, 20);
        oledShared->show();
        xEventGroupSetBits(eventGroup, BIT_TASK_HMP);
        vTaskDelay(pdMS_TO_TICKS(8000));
    }
}

[[noreturn]] void modbusControlTask(void *pvParameters) {
    auto *debug = static_cast<Debug *>(pvParameters);
    ModbusMIO modbusSystem(modbus, modbusMutex, oledShared);

    while (true) {
        // int desiredValue = encoderHandler.getDesiredValue();
        const int desiredValue = co2Input.getDesiredValue();
        const int co2Value = static_cast<int>(gmp252.readMeasuredCO2());

        modbusSystem.controlLoop(gmp252, desiredValue); // handles fan + valve + display

        char buf[128];
        snprintf(buf, sizeof(buf), "CO2=%d ppm, Desired=%d ppm\n", co2Value, desiredValue);
        debug->print(buf);

        xEventGroupSetBits(eventGroup, BIT_TASK_FAN);
        vTaskDelay(pdMS_TO_TICKS(4000));
    }
}

[[noreturn]] void watchDogTimer(void *pvParameters) {
    auto *debug = static_cast<Debug *>(pvParameters);
    TickType_t lastOK = xTaskGetTickCount();

    while (true) {
        EventBits_t result = xEventGroupWaitBits(eventGroup, ALL_TASK_BITS, pdTRUE, pdTRUE, pdMS_TO_TICKS(30000));
        char wdStatus[32];
        snprintf(wdStatus, sizeof(wdStatus), "W.D.[%c,%c,%c]",
                 (result & BIT_TASK_FAN) ? '1' : '#',
                 (result & BIT_TASK_GMP) ? '2' : '#',
                 (result & BIT_TASK_HMP) ? '3' : '#');

        if ((result & ALL_TASK_BITS) == ALL_TASK_BITS) {
            TickType_t now = xTaskGetTickCount();
            char buf[128];
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

        oledShared->text(wdStatus, 2, 40);
        oledShared->show();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// --- Main Function ---
int main() {
    stdio_init_all();
    init_function();

    oledShared->fill(0);
    auto debug = std::make_shared<Debug>();
    auto debugTask = std::make_unique<DebugTask>(debug);
    debug->print("Program started.\n");
    co2Input.startTasks(oledShared);
    // Create tasks
    xTaskCreate(watchDogTimer, "WatchDogTimer", 1024, debug.get(), tskIDLE_PRIORITY + 1, nullptr);
    xTaskCreate(modbusGmpTask, "CO2Task", 512, debug.get(), tskIDLE_PRIORITY + 1, nullptr);
    xTaskCreate(modbusHmpTask, "HMPTask", 512, debug.get(), tskIDLE_PRIORITY + 1, nullptr);
    xTaskCreate(modbusControlTask, "ControlTask", 512, debug.get(), tskIDLE_PRIORITY + 2, nullptr); // highest priority

    vTaskStartScheduler();

    while (true) {
    } // Should never reach here
}
