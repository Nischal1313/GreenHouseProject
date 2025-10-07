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
#include "eeprom.h"
#include "encoderHandler.h"
#include "inputManager.h"


extern "C" {
    uint32_t read_runtime_ctr(void) {
        return time_us_32();
    }
}

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
// --- Modbus Control Task ---
[[noreturn]] void modbusControlTask(void* pvParameters) {
    const auto* debug = static_cast<Debug*>(pvParameters);
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

int main() {
    stdio_init_all();
    printf("1 - System Init Start\n");

    // --- I2C setup ---
    i2c_init(i2c1, 400000);
    gpio_set_function(14, GPIO_FUNC_I2C);
    gpio_set_function(15, GPIO_FUNC_I2C);
    gpio_pull_up(14);
    gpio_pull_up(15);

    // --- Modbus setup ---
    modbusMutex = xSemaphoreCreateMutex();
    uart = std::make_shared<PicoOsUart>(1, 4, 5, 9600, 8, 256, 256);
    modbusClient = std::make_shared<ModbusClient>(uart);
    gmpSensor = new GMP252(modbusClient, modbusMutex);
    hmpSensor = new HMP60(modbusClient, modbusMutex);
    modbusSystem = new ModbusMIO(modbusClient, modbusMutex);

    encoder = new RotaryEncoder();

    // --- EEPROM + credentials setup ---
    Eeprom eeprom(i2c0, 0x50, 2); // EEPROM on I2C0
    SemaphoreHandle_t eepromMutex = xSemaphoreCreateMutex();
    SetCredentials credentials(eeprom, eepromMutex);

    // --- Input manager setup ---
    InputManager inputManager{}; // GPIO pins for buttons

    // --- Display manager setup ---
    DisplayParams displayParams {
        gmpSensor,
        hmpSensor,
        modbusSystem,
        encoder,
        &inputManager,
        &credentials
    };

    DisplayManager displayManager;
    displayManager.setParams(&displayParams);

    // --- Task creation ---
    // xTaskCreate(modbusControlTask, "modbusControlTask", 2048, nullptr, 2, nullptr);
    xTaskCreate(InputManager::taskEntry, "InputTask", 512, &inputManager, tskIDLE_PRIORITY + 3, nullptr);
    xTaskCreate(DisplayManager::taskEntry, "DisplayTask", 2048, &displayManager, tskIDLE_PRIORITY + 2, nullptr);
    // xTaskCreate(RotaryEncoder::encoderTask, "EncoderPoll", 512, encoder, tskIDLE_PRIORITY + 2, nullptr);

    vTaskStartScheduler();
}
