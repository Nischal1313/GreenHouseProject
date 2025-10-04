#include <cmath>
#include <cstdio>
#include <memory>
#include "FreeRTOS.h"
#include "task.h"
#include "hardware/gpio.h"
#include "pico/stdio.h"

#include "PicoOsUart.h"
#include "PicoI2C.h"
#include "ssd1306os.h"

#include "gmp252.h"
#include "hmp60.h"
#include "produalMIO.h"
#include "displayMenu.h"
#include "rotaryEncoder.h"
#include "debug.h"
#include "event_groups.h"


extern "C" {
    uint32_t read_runtime_ctr(void) {
        return time_us_32();
    }
}

// Global resources
const SemaphoreHandle_t modbusMutex = xSemaphoreCreateMutex();
auto uart = std::make_shared<PicoOsUart>(1, 4, 5, 9600, 8, 256, 256);
auto modbusClient = std::make_shared<ModbusClient>(uart);
EventGroupHandle_t eventGroup;

// Sensors / actuators
GMP252 gmpSensor(modbusClient, modbusMutex);
HMP60 hmpSensor(modbusClient, modbusMutex);
RELAYCONTROL valve;
ModbusMIO modbusSystem(modbusClient, modbusMutex);
RotaryEncoder encoder;

// I2C & OLED
auto i2cBus = std::make_shared<PicoI2C>(1, 400000);
auto oledShared = std::make_shared<ssd1306os>(i2cBus);

// Display manager
DisplayParams displayParams{
    i2cBus,
    oledShared,
    &gmpSensor,
    &hmpSensor,
    &modbusSystem,
    &encoder,
    &valve
};
DisplayManager displayManager(displayParams);

// --- Tasks ---
[[noreturn]] void displayTask(void *pvParameters) {
    static_cast<DisplayManager*>(pvParameters)->displayTask();
}

[[noreturn]] void modbusControlTask(void *pvParameters) {
    const auto *debug = static_cast<Debug*>(pvParameters);

    while (true) {
        const int desiredCO2 = encoder.currentRotationValue();
        const int currentCO2 = static_cast<int>(gmpSensor.readMeasuredCO2());

        modbusSystem.controlLoop(gmpSensor, encoder);

        char buf[128];
        snprintf(buf, sizeof(buf), "CO2=%d ppm, Desired=%d ppm\n", currentCO2, desiredCO2);
        debug->print(buf);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// --- Main ---
int main() {
    stdio_init_all();
    // Debug (turn on the DTR to see debug)
    auto debug{std::make_shared<Debug>()};
    auto debugTask{std::make_unique<DebugTask>(debug)};
    oledShared->fill(0);
    debug->print("Program started.\n");


    // FreeRTOS tasks
    xTaskCreate(displayTask, "DisplayTask", 2048, &displayManager, tskIDLE_PRIORITY + 2, nullptr);
    xTaskCreate(modbusControlTask, "ControlTask", 1024, debug.get(), tskIDLE_PRIORITY + 3, nullptr);

    vTaskStartScheduler();

    while(true) {} // Should never reach
}
