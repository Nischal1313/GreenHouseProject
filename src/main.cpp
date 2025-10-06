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
//
// // Global resources
// const SemaphoreHandle_t modbusMutex = xSemaphoreCreateMutex();
// auto uart = std::make_shared<PicoOsUart>(1, 4, 5, 9600, 8, 256, 256);
// auto modbusClient = std::make_shared<ModbusClient>(uart);
// EventGroupHandle_t eventGroup;
//
// // Sensors / actuators
// GMP252 gmpSensor(modbusClient, modbusMutex);
// HMP60 hmpSensor(modbusClient, modbusMutex);
// ModbusMIO modbusSystem(modbusClient, modbusMutex);
// RotaryEncoder encoder;
// RELAYCONTROL valve;
//
//
// // Display manager
// DisplayParams displayParams{
//     &gmpSensor,
//     &hmpSensor,
//     &modbusSystem,
//     &encoder,
//     &valve
// };
//
// DisplayManager displayManager(displayParams);


// --- Global Resources ---
SemaphoreHandle_t modbusMutex;
std::shared_ptr<PicoOsUart> uart;
std::shared_ptr<ModbusClient> modbusClient;

// CHANGE: Use pointers for Modbus-dependent and I2C-dependent objects
GMP252* gmpSensor = nullptr;
HMP60* hmpSensor = nullptr;
ModbusMIO* modbusSystem = nullptr;
RotaryEncoder* encoder = nullptr; // RotaryEncoder uses I2C for EEPROM

// --- Tasks ---
[[noreturn]] void displayTask(void *pvParameters) {
    static_cast<DisplayManager*>(pvParameters)->displayTask();
}

// [[noreturn]] void modbusControlTask(void *pvParameters) {
//     const auto *debug = static_cast<Debug*>(pvParameters);
//
//     while (true) {
//         const int desiredCO2 = encoder.currentRotationValue();
//         const int currentCO2 = static_cast<int>(gmpSensor.readMeasuredCO2());
//
//         modbusSystem.controlLoop(gmpSensor, encoder);
//
//         char buf[128];
//         snprintf(buf, sizeof(buf), "CO2=%d ppm, Desired=%d ppm\n", currentCO2, desiredCO2);
//         debug->print(buf);
//
//         vTaskDelay(pdMS_TO_TICKS(1000));
//     }
// }
// --- Modbus Control Task ---
[[noreturn]] void modbusControlTask(void* pvParameters) {
    const auto* debug = static_cast<Debug*>(pvParameters);

    // extern declarations are no longer strictly needed if defined globally
    // extern GMP252 gmpSensor; // DELETE
    // extern RotaryEncoder encoder; // DELETE
    // extern ModbusMIO modbusSystem; // DELETE
    extern GMP252* gmpSensor; // USE POINTER
    extern RotaryEncoder* encoder; // USE POINTER
    extern ModbusMIO* modbusSystem; // USE POINTER

    while (true) {
        // Use -> to access members
        const int desiredCO2 = encoder->currentRotationValue();
        const int currentCO2 = static_cast<int>(gmpSensor->readMeasuredCO2());

        modbusSystem->controlLoop(*gmpSensor, *encoder); // Pass dereferenced objects

        char buf[128];
        snprintf(buf, sizeof(buf), "CO2=%d ppm, Desired=%d ppm\n", currentCO2, desiredCO2);
        debug->print(buf);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
// --- Main ---
// --- Main ---
int main() {
    stdio_init_all();
    printf("1 - System Init Start\n");

    // 1. Initialize I2C Bus (Dependency for RotaryEncoder/EEPROM and DisplayManager)
    // Move this initialization from DisplayManager constructor to here.
    i2c_init(i2c1, 400000);
    gpio_set_function(14, GPIO_FUNC_I2C);
    gpio_set_function(15, GPIO_FUNC_I2C);
    gpio_pull_up(14);
    gpio_pull_up(15);
    printf("2 - I2C Initialized\n");

    // 2. Initialize Modbus Dependencies
    modbusMutex = xSemaphoreCreateMutex();
    uart = std::make_shared<PicoOsUart>(1, 4, 5, 9600, 8, 256, 256);
    modbusClient = std::make_shared<ModbusClient>(uart);
    printf("3 - Modbus Client Initialized\n");

    // 3. Initialize Dependent Objects *after* all dependencies are ready
    // Remove placement new attempts
    gmpSensor = new GMP252(modbusClient, modbusMutex);
    hmpSensor = new HMP60(modbusClient, modbusMutex);
    modbusSystem = new ModbusMIO(modbusClient, modbusMutex);
    encoder = new RotaryEncoder(); // This now runs after I2C is ready (rotaryEncoder.cpp)
    printf("4 - Hardware Objects Constructed\n");

    // 4. Display setup - Use the pointers
    DisplayParams displayParams{gmpSensor, hmpSensor, modbusSystem, encoder};
    DisplayManager displayManager;
    displayManager.setParams(&displayParams);

    // ... Debug setup ...
    auto debug = std::make_shared<Debug>();
    auto debugTask = std::make_unique<DebugTask>(debug);
    debug->print("Program started.\n");
    printf("5 - Debug and Display Ready\n");

    // 5. Create tasks *after* all objects are initialized
    xTaskCreate(RotaryEncoder::encoderTask, "EncoderPoll", 512, encoder, tskIDLE_PRIORITY + 2, nullptr); // NEW TASK
    xTaskCreate(DisplayManager::taskEntry, "DisplayTask", 2048, &displayManager, tskIDLE_PRIORITY + 2, nullptr);
    xTaskCreate(modbusControlTask, "ControlTask", 2048, debug.get(), tskIDLE_PRIORITY + 3, nullptr);
    printf("6 - Tasks Created, Starting Scheduler...\n");

    // Start scheduler
    vTaskStartScheduler();
    // ...
}