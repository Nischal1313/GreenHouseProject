#include <cmath>
#include <cstdio>
#include <memory>
#include "FreeRTOS.h"
#include "task.h"
#include "hardware/gpio.h"
#include "pico/stdio.h"

#include "PicoOsUart.h"
#include "PicoI2C.h"
#include "displayMenu.h"
#include "rotary_encoder.h"
#include "debug.h"
#include "event_groups.h"
#include "inputManager.h"
#include "ModbusClient.h"
#include "nanomodbus.h"
#include "eeprom/eeprom.h"
#include "sensor_handler.h"
#include "setpoint_manager.h"
#include "setCredentials.h"
#include "cloud_handler.h"
#include "ssd1306os.h"

extern "C" {
uint32_t read_runtime_ctr(void) {
  return time_us_32();
}
}

// Encoder update task - polls encoder hardware every 5ms
[[noreturn]] void encoderUpdateTask(void *pvParameters) {
  auto *encoder = static_cast<RotaryEncoder *>(pvParameters);
  printf("[EncoderTask] Started\n");
  while (true) {
    encoder->update();
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}


[[noreturn]] int main() {
  stdio_init_all();
  // --- I2C setup for OLED (i2c1) ---
  printf("1 - Initializing I2C1 for OLED...\n");
  i2c_init(i2c1, 400'000);
  gpio_set_function(14, GPIO_FUNC_I2C);
  gpio_set_function(15, GPIO_FUNC_I2C);
  gpio_pull_up(14);
  gpio_pull_up(15);
  auto i2cbus = std::make_shared<PicoI2C>(1, 400'000);
  auto oLed = std::make_shared<ssd1306os>(i2cbus);
  printf("   OLED initialized\n");

  // --- I2C setup for EEPROM (i2c0) ---
  printf("2 - Initializing I2C0 for EEPROM...\n");
  i2c_init(i2c0, 400'000);
  gpio_set_function(16, GPIO_FUNC_I2C);
  gpio_set_function(17, GPIO_FUNC_I2C);
  gpio_pull_up(16);
  gpio_pull_up(17);
  auto eeprom = std::make_shared<Eeprom>(i2c0, 0x50, 2);
  printf("   EEPROM initialized\n");

  // --- Create mutexes ---
  printf("3 - Creating mutexes...\n");
  SemaphoreHandle_t eepromMutex = xSemaphoreCreateMutex();
  SemaphoreHandle_t sensorMutex = xSemaphoreCreateMutex();
  printf("   Mutexes created\n");

  // --- Initialize Debug system ---
  printf("4 - Initializing Debug system...\n");
  auto debug = std::make_shared<Debug>();
  auto debugTask = std::make_shared<DebugTask>(debug);
  debug->print("Debug system online\n");

  // --- Initialize Rotary Encoder (SINGLE SHARED INSTANCE) ---
  printf("5 - Initializing Rotary Encoder...\n");
  auto encoder = std::make_shared<RotaryEncoder>();

  // --- Initialize Input Manager (GPIO buttons) ---
  printf("6 - Initializing Input Manager...\n");
  auto inputManager = std::make_shared<InputManager>();

  // --- Initialize Credentials Manager ---
  printf("7 - Initializing Credentials Manager...\n");
  auto credentials = std::make_shared<SetCredentials>(*eeprom, eepromMutex);

  // --- Initialize SensorHandler ---
  printf("9 - Initializing SensorHandler...\n");
  auto sensorHandler = std::make_shared<SensorHandler>(sensorMutex, encoder,
    eepromMutex, *eeprom);
  printf("   Sensor handler initialized\n");

  static DisplayManager displayManager(debug, oLed, encoder);

  // --- Prepare DisplayParams ---
  DisplayParams displayParams{
    .sensorHandler = sensorHandler.get(),
    .credentials = credentials.get(),
    .inputManager = inputManager.get(),
    .oLed = oLed.get(),
    .encoder = encoder.get()
  };

  // --- Set parameters ---
  displayManager.setParams(&displayParams);


  // --- Create FreeRTOS Tasks ---
  printf("12 - Creating FreeRTOS tasks...\n");

  xTaskCreate(encoderUpdateTask, "EncoderHW", 1024,
    encoder.get(), 3, nullptr);

  xTaskCreate(SensorHandler::controlTask, "SensorCtrl", 2048,
    sensorHandler.get(), 2, nullptr);

  xTaskCreate(DisplayManager::taskEntry, "DisplayTask", 2048,
    &displayManager, 1, nullptr);

  xTaskCreate(InputManager::taskEntry, "InputMgr", 1024,
    inputManager.get(), 3, nullptr);

  vTaskStartScheduler();

  // Should never reach here
  while (true) {}
}
//
// #include "eeprom/eeprom.h"
// #include <cstdio>
// #include <memory>
// #include "pico/stdlib.h"
//
// int main() {
//   stdio_init_all();
//
//   printf("Initializing I2C0 for EEPROM...\n");
//   i2c_init(i2c0, 400000);
//   gpio_set_function(16, GPIO_FUNC_I2C);
//   gpio_set_function(17, GPIO_FUNC_I2C);
//   gpio_pull_up(16);
//   gpio_pull_up(17);
//
//   auto eeprom = std::make_shared<Eeprom>(i2c0, 0x50, 2);
//   printf("EEPROM initialized\n");
//
//   const uint16_t address = 0x000;
//   uint16_t value = 222;
//
//   printf("EEPROM integer write/read test\n");
//
//   while (true) {
//     // Write value
//     uint8_t writeBuf[2] = {
//       static_cast<uint8_t>(value >> 8),
//       static_cast<uint8_t>(value & 0xFF)
//   };
//
//     if (eeprom->writeBlock(address, writeBuf, 2))
//       printf("[WRITE] %u\n", value);
//     else
//       printf("[WRITE FAILED]\n");
//
//     sleep_ms(3000);
//
//     // Read value
//     uint8_t readBuf[2] = {0};
//     if (eeprom->readBlock(address, readBuf, 2)) {
//       uint16_t readVal = (readBuf[0] << 8) | readBuf[1];
//       printf("[READ] %u\n", readVal);
//     } else {
//       printf("[READ FAILED]\n");
//     }
//
//     sleep_ms(3000);
//     value++;
//   }
// }
