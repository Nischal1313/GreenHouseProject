// #include <cstdio>
// #include <memory>
// #include "FreeRTOS.h"
// #include "task.h"
// #include "semphr.h"
// #include "hardware/gpio.h"
// #include "pico/stdio.h"
// #include "hardware/i2c.h"
// #include "pico/stdlib.h"
//
// #include "PicoOsUart.h"
// #include "PicoI2C.h"
// #include "ssd1306os.h"
//
// #include "gmp252.h"
// #include "hmp60.h"
// #include "produalMIO.h"
// #include "displayMenu.h"
// #include "rotaryEncoder.h"
// #include "debug.h"
// #include "event_groups.h"
//
// #include "setCredentials.h"
// #include "eeprom.h"
//
// extern "C" {
// uint32_t read_runtime_ctr(void) {
//     return time_us_32();
// }
// }
//
// SemaphoreHandle_t modbusMutex;
// std::shared_ptr<PicoOsUart> uart;
// std::shared_ptr<ModbusClient> modbusClient;
//
// // CHANGE: Use pointers for Modbus-dependent and I2C-dependent objects
// GMP252 *gmpSensor = nullptr;
// HMP60 *hmpSensor = nullptr;
// ModbusMIO *modbusSystem = nullptr;
// RotaryEncoder *encoder = nullptr; // RotaryEncoder uses I2C for EEPROM
//
// // --- Tasks ---
// [[noreturn]] void displayTask(void *pvParameters) {
//     static_cast<DisplayManager *>(pvParameters)->displayTask();
// }
//
// [[noreturn]] void modbusControlTask(void *pvParameters) {
//     const auto *debug = static_cast<Debug *>(pvParameters);
//
//     // extern declarations are no longer strictly needed if defined globally
//     // extern GMP252 gmpSensor; // DELETE
//     // extern RotaryEncoder encoder; // DELETE
//     // extern ModbusMIO modbusSystem; // DELETE
//     extern GMP252 *gmpSensor; // USE POINTER
//     extern RotaryEncoder *encoder; // USE POINTER
//     extern ModbusMIO *modbusSystem; // USE POINTER
//     // ---------------------
//     //  Task Declarations
//     // ---------------------
// }
//
// [[noreturn]] void modbusControlTask(void *pvParameters) {
//     auto *params = static_cast<DisplayParams *>(pvParameters);
//
//     while (true) {
//         // Use -> to access members
//         const int desiredCO2 = encoder->currentRotationValue();
//         const int currentCO2 = static_cast<int>(gmpSensor->readMeasuredCO2());
//         Debug debug;
//         int desiredCO2 = params->encoder->currentRotationValue();
//         int currentCO2 = static_cast<int>(params->gmpSensor->readMeasuredCO2());
//
//         modbusSystem->controlLoop(*gmpSensor, *encoder); // Pass dereferenced objects
//         params->modbusSystem->controlLoop(*params->gmpSensor, *params->encoder);
//
//         char buf[128];
//         snprintf(buf, sizeof(buf), "CO2=%d ppm, Desired=%d ppm\n", currentCO2, desiredCO2);
//         debug->print(buf);
//         debug.print(buf);
//
//         vTaskDelay(pdMS_TO_TICKS(1000));
//     }
// }
//
//
// // --- Main ---
// // --- Main ---
//
// // ---------------------
// //  Main Entry Point
// // ---------------------
// int main() {
//     stdio_init_all();
//     printf("1 - System Init Start\n");
//
//     // 1. Initialize I2C Bus (Dependency for RotaryEncoder/EEPROM and DisplayManager)
//     // Move this initialization from DisplayManager constructor to here.
//     i2c_init(i2c1, 400000);
//     gpio_set_function(14, GPIO_FUNC_I2C);
//     gpio_set_function(15, GPIO_FUNC_I2C);
//     gpio_pull_up(14);
//     gpio_pull_up(15);
//     printf("2 - I2C Initialized\n");
//
//     // 2. Initialize Modbus Dependencies
//     modbusMutex = xSemaphoreCreateMutex();
//     uart = std::make_shared<PicoOsUart>(1, 4, 5, 9600, 8, 256, 256);
//     modbusClient = std::make_shared<ModbusClient>(uart);
//     printf("3 - Modbus Client Initialized\n");
//
//     // 3. Initialize Dependent Objects *after* all dependencies are ready
//     // Remove placement new attempts
//     gmpSensor = new GMP252(modbusClient, modbusMutex);
//     hmpSensor = new HMP60(modbusClient, modbusMutex);
//     modbusSystem = new ModbusMIO(modbusClient, modbusMutex);
//     encoder = new RotaryEncoder(); // This now runs after I2C is ready (rotaryEncoder.cpp)
//     printf("4 - Hardware Objects Constructed\n");
//
//     // 4. Display setup - Use the pointers
//     DisplayParams displayParams{gmpSensor, hmpSensor, modbusSystem, encoder};
//     printf("System startup...\n");
//
//     // 1. Initialize I2C0 for EEPROM
//     i2c_init(i2c0, 400000);
//     gpio_set_function(16, GPIO_FUNC_I2C);
//     gpio_set_function(17, GPIO_FUNC_I2C);
//     gpio_pull_up(16);
//     gpio_pull_up(17);
//     printf("I2C0 initialized for EEPROM\n");
//
//     // 2. Create EEPROM mutex for thread-safe access
//     SemaphoreHandle_t eepromMutex = xSemaphoreCreateMutex();
//     if (eepromMutex == nullptr) {
//         printf("ERROR: Failed to create EEPROM mutex\n");
//         while (1) { tight_loop_contents(); }
//     }
//
//     // 3. Create shared EEPROM instance
//     Eeprom eeprom(i2c0, 0x50, 2);
//     printf("EEPROM instance created\n");
//
//     // 4. Initialize UART & Modbus
//     SemaphoreHandle_t modbusMutex = xSemaphoreCreateMutex();
//     auto uart = std::make_shared<PicoOsUart>(1, 4, 5, 9600, 8, 256, 256);
//     auto modbusClient = std::make_shared<ModbusClient>(uart);
//     printf("Modbus initialized\n");
//
//     // 5. Hardware-dependent objects
//     GMP252 gmpSensor(modbusClient, modbusMutex);
//     HMP60 hmpSensor(modbusClient, modbusMutex);
//     ModbusMIO modbusSystem(modbusClient, modbusMutex);
//
//     // 6. Create RotaryEncoder and SetCredentials with shared EEPROM and mutex
//     RotaryEncoder encoder(eeprom, eepromMutex);
//     SetCredentials credentials(eeprom, eepromMutex);
//     printf("Encoder and credentials initialized\n");
//
//     // 7. Display Manager and parameter linkage
//     DisplayParams displayParams{
//         &gmpSensor,
//         &hmpSensor,
//         &modbusSystem,
//         &encoder
//     };
//     DisplayManager displayManager;
//     displayManager.setParams(&displayParams);
//
//     // ... Debug setup ...
//     auto debug = std::make_shared<Debug>();
//     auto debugTask = std::make_unique<DebugTask>(debug);
//     debug->print("Program started.\n");
//     printf("5 - Debug and Display Ready\n");
//     // 8. GPIO setup for buttons (7–9)
//     const uint buttonPins[] = {7, 8, 9};
//     for (uint pin: buttonPins) {
//         gpio_init(pin);
//         gpio_set_dir(pin, GPIO_IN);
//         gpio_pull_up(pin);
//     }
//
//     // Attach IRQs
//     gpio_set_irq_enabled_with_callback(7, GPIO_IRQ_EDGE_FALL, true, &DisplayManager::buttonIRQ);
//     gpio_set_irq_enabled(8, GPIO_IRQ_EDGE_FALL, true);
//     gpio_set_irq_enabled(9, GPIO_IRQ_EDGE_FALL, true);
//
//     // 5. Create tasks *after* all objects are initialized
//     xTaskCreate(RotaryEncoder::encoderTask, "EncoderPoll", 512, encoder, tskIDLE_PRIORITY + 2, nullptr); // NEW TASK
//     // 9. Start tasks
//     xTaskCreate(RotaryEncoder::encoderTask, "EncoderPoll", 512, &encoder, tskIDLE_PRIORITY + 2, nullptr);
//     xTaskCreate(DisplayManager::taskEntry, "DisplayTask", 2048, &displayManager, tskIDLE_PRIORITY + 2, nullptr);
//     xTaskCreate(modbusControlTask, "ControlTask", 2048, debug.get(), tskIDLE_PRIORITY + 3, nullptr);
//     printf("6 - Tasks Created, Starting Scheduler...\n");
//     xTaskCreate(modbusControlTask, "ControlTask", 2048, &displayParams, tskIDLE_PRIORITY + 3, nullptr);
//
//     // Start scheduler
//     printf("Tasks running. Scheduler start...\n");
//     vTaskStartScheduler();
//     // ...
//
//     // Should never reach here
//     while (1) { tight_loop_contents(); }
// }
