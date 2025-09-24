#include <cmath>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <memory>
#include "FreeRTOS.h"
#include "event_groups.h"
#include "gmp252.h"
#include "pico/stdlib.h"
#include "task.h"
#include "hardware/gpio.h"
#include "uart/PicoOsUart.h"
#include "modbus/ModbusClient.h"
#include "semphr.h"
#include "produalMIO.h"
#include "mutexGuard.h"
#include "hmp60.h"
#include "sdp610.h"
#include "IPStack.h"

//---------------------------------------------------
// Runtime counter required by some drivers
//---------------------------------------------------
extern "C" {
uint32_t read_runtime_ctr(void) {
    return time_us_32();
}
}

//---------------------------------------------------
// Task priorities
//---------------------------------------------------
constexpr int TASK_HIGH_PRIORITY = 2 + tskIDLE_PRIORITY;
constexpr int WATCHDOG_PRIORITY = 2 + tskIDLE_PRIORITY;
constexpr int TASK_LOW_PRIORITY = 1 + tskIDLE_PRIORITY;

//---------------------------------------------------
// Buttons
//---------------------------------------------------
constexpr int BUTTON1_PIN = 7;
constexpr int BUTTON2_PIN = 8;
constexpr int BUTTON3_PIN = 9;

//---------------------------------------------------
// Event group bits
//---------------------------------------------------
constexpr EventBits_t BIT_TASK_FAN = (1 << 0);
constexpr EventBits_t BIT_TASK_GMP = (1 << 1);
constexpr EventBits_t BIT_TASK_HMP = (1 << 2);
constexpr EventBits_t BIT_TASK_SDP = (1 << 3);
constexpr EventBits_t ALL_TASK_BITS = BIT_TASK_FAN | BIT_TASK_GMP | BIT_TASK_HMP | BIT_TASK_SDP;

//---------------------------------------------------
// I2C instance
//---------------------------------------------------
i2c_inst_t *i2c_instance = i2c1;
constexpr uint SDA_PIN = 14;
constexpr uint SCL_PIN = 15;

//---------------------------------------------------
// Global handles
//---------------------------------------------------
SemaphoreHandle_t modbusMutex;
SemaphoreHandle_t i2cMutex;
SemaphoreHandle_t buttonMutex;

std::shared_ptr<PicoOsUart> uart;
std::shared_ptr<ModbusClient> modbus;

ModbusMIO *modbusFan;
GMP252 *gmpSensor;
HMP60 *hmpSensor;
SDP610 *pressureSensor;

QueueHandle_t syslog_q;
constexpr int DEBUG_QUEUE_LENGTH = 10;

EventGroupHandle_t eventGroup;

constexpr TickType_t WATCHDOG_TIMER = pdMS_TO_TICKS(30000);

//---------------------------------------------------
// Debug event structure
//---------------------------------------------------
struct debugEvent {
    char debug[128];
    uint32_t timestamp;
};

//---------------------------------------------------
// Debug helper
//---------------------------------------------------
void debug(const char *message) {
    debugEvent e{};
    e.timestamp = xTaskGetTickCount();
    snprintf(e.debug, sizeof(e.debug), "%s", message);
    xQueueSend(syslog_q, &e, pdMS_TO_TICKS(10));
}

//---------------------------------------------------
// Initialization
//---------------------------------------------------
void initFunction() {
    stdio_init_all();
    // Buttons
    gpio_init(BUTTON1_PIN);
    gpio_set_dir(BUTTON1_PIN, GPIO_IN);
    gpio_pull_up(BUTTON1_PIN);
    gpio_init(BUTTON2_PIN);
    gpio_set_dir(BUTTON2_PIN, GPIO_IN);
    gpio_pull_up(BUTTON2_PIN);
    gpio_init(BUTTON3_PIN);
    gpio_set_dir(BUTTON3_PIN, GPIO_IN);
    gpio_pull_up(BUTTON3_PIN);

    // Create mutexes
    modbusMutex = xSemaphoreCreateMutex();
    i2cMutex = xSemaphoreCreateMutex();
    buttonMutex = xSemaphoreCreateMutex();
    // Event group and debug queue
    eventGroup = xEventGroupCreate();
    syslog_q = xQueueCreate(DEBUG_QUEUE_LENGTH, sizeof(debugEvent));
    // UART hardware init
    uart_init(uart1, 9600);
    gpio_set_function(4, GPIO_FUNC_UART);
    gpio_set_function(5, GPIO_FUNC_UART);

    uart_set_format(uart1, 8, 1, UART_PARITY_EVEN);

    // UART wrapper and Modbus client
    uart = std::make_shared<PicoOsUart>(1, 4, 5, 9600, 8, 256, 256);
    modbus = std::make_shared<ModbusClient>(uart);

    // ModbusMIO (fan control)
    modbusFan = new ModbusMIO(modbus, modbusMutex);

    // Sensors
    gmpSensor = new GMP252(modbus, modbusMutex); // Slave 240
    hmpSensor = new HMP60(modbus, modbusMutex);

    // Initialize I2C1 for SDP610
    i2c_init(i2c_instance, 100 * 1000); // 100 kHz
    // Create SDP610 instance (do NOT call init() here; the task will call / or you can call it here)
    pressureSensor = new SDP610(i2c_instance, SDA_PIN, SCL_PIN, i2cMutex, 0x40);

    // Initialize sensor once
    if (!pressureSensor->init()) {
        debug("Failed to initialize SDP610 pressure sensor\n");
    }
}

//---------------------------------------------------
// Tasks
//---------------------------------------------------
[[noreturn]] void debugTask(void *pvParameters) {
    debugEvent e{};
    while (true) {
        if (xQueueReceive(syslog_q, &e, portMAX_DELAY) == pdPASS) {
            // Print with newline (debug strings passed in already include newline in many places)
            std::cout << "[" << e.timestamp << "] " << e.debug << std::flush;
        }
    }
}

[[noreturn]] void watchDogTimer(void *pvParameter) {
    TickType_t lastOK = xTaskGetTickCount();
    while (true) {
        EventBits_t result = xEventGroupWaitBits(eventGroup, ALL_TASK_BITS, pdTRUE, pdTRUE, WATCHDOG_TIMER);
        if ((result & ALL_TASK_BITS) == ALL_TASK_BITS) {
            TickType_t now = xTaskGetTickCount();
            char buf[128];
            snprintf(buf, sizeof(buf), "Watchdog: OK, %lu ms since last OK\n",
                     static_cast<unsigned long>((now - lastOK) * portTICK_PERIOD_MS));
            debug(buf);
            lastOK = now;
        } else {
            EventBits_t missingBits = ALL_TASK_BITS & ~result;
            debug("Watchdog FAIL! Missing tasks: -> ");
            if (missingBits & BIT_TASK_FAN) debug("  Fan control task\n");
            if (missingBits & BIT_TASK_GMP) debug("  GMP252 CO2 sensor task\n");
            if (missingBits & BIT_TASK_HMP) debug("  HMP60 humidity/temp task\n");
            if (missingBits & BIT_TASK_SDP) debug("  SDP610 pressure sensor task\n");
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}

[[noreturn]] void modbusFanTask(void *pvParameters) {
    constexpr TickType_t taskDelay = pdMS_TO_TICKS(50000);
    while (true) {
        constexpr float desiredFanSpeed = 0.0f;
        bool success = modbusFan->setFanSpeed(desiredFanSpeed);
        bool fanRunning = modbusFan->isFanRunning();

        char buf[128];
        if (success) {
            snprintf(buf, sizeof(buf), "Fan speed set to %.1f%%, fan is running\n", desiredFanSpeed);
            debug(buf);
        }
        if (fanRunning) {
            snprintf(buf, sizeof(buf), "Fan is running.\n");
            debug(buf);
        } else {
            snprintf(buf, sizeof(buf), "Failed to set speed.\n");
            debug(buf);
        }

        xEventGroupSetBits(eventGroup, BIT_TASK_FAN);
        vTaskDelay(taskDelay);
    }
}

[[noreturn]] void modbusGmpTask(void *pvParameters) {
    constexpr TickType_t taskDelay = pdMS_TO_TICKS(2000);
    while (true) {
        const float co2 = gmpSensor->readMeasuredCO2();
        if (!std::isnan(co2)) {
            char buf[128];
            snprintf(buf, sizeof(buf),
                     "GMP252 - CO2: %.1f ppm\n", co2);
            debug(buf);
        } else debug("GMP252 sensor read failed!\n");

        xEventGroupSetBits(eventGroup, BIT_TASK_GMP);
        vTaskDelay(taskDelay);
    }
}

[[noreturn]] void modbusHmpTask(void *pvParameters) {
    constexpr TickType_t taskDelay = pdMS_TO_TICKS(3000);
    while (true) {
        const float hum = hmpSensor->readHumidity();
        const float temp = hmpSensor->readTemperature();

        if (!std::isnan(hum) && !std::isnan(temp)) {
            char buf[128];
            snprintf(buf, sizeof(buf), "HMP60 - Humidity: %.1f%%, Temperature: %.1fC\n", hum, temp);
            debug(buf);
        } else debug("HMP60 sensor read failed!\n");

        xEventGroupSetBits(eventGroup, BIT_TASK_HMP);
        vTaskDelay(taskDelay);
    }
}

[[noreturn]] void I2cPressureSensorTask(void *pvParameters) {
    constexpr TickType_t taskDelay = pdMS_TO_TICKS(4000);

    // The sensor was initialized in initFunction(); if you prefer to init here instead,
    // remove the init call in initFunction() and call pressureSensor->init() here.
    while (true) {
        float pressure = pressureSensor->readPressurePa();
        if (!std::isnan(pressure)) {
            char buf[128];
            snprintf(buf, sizeof(buf), "SDP610 - Pressure: %.2f Pa\n", pressure);
            debug(buf);
        } else {
            debug("SDP610 pressure sensor read failed!\n");
        }

        xEventGroupSetBits(eventGroup, BIT_TASK_SDP);
        vTaskDelay(taskDelay);
    }
}

//---------------------------------------------------
// Main
//---------------------------------------------------
[[noreturn]] int main() {
    initFunction();
    debug("Program started.\n");

    xTaskCreate(debugTask, "DebugTask", 1024, nullptr, TASK_LOW_PRIORITY, nullptr);
    xTaskCreate(watchDogTimer, "WatchDogTimer", 1024, nullptr, WATCHDOG_PRIORITY, nullptr);
    xTaskCreate(modbusFanTask, "ModbusFanTask", 1024, nullptr, TASK_HIGH_PRIORITY, nullptr);
    xTaskCreate(modbusGmpTask, "ModbusGmpTask", 1024, nullptr, TASK_HIGH_PRIORITY, nullptr);
    xTaskCreate(modbusHmpTask, "ModbusHmpTask", 1024, nullptr, TASK_HIGH_PRIORITY, nullptr);
    xTaskCreate(I2cPressureSensorTask, "PressureSensorTask", 1024, nullptr, TASK_HIGH_PRIORITY, nullptr);

    debug("All tasks created. Starting scheduler.\n");

    vTaskStartScheduler();
    while (true);
}
