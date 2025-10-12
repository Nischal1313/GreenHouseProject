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

extern "C" {
uint32_t read_runtime_ctr(void) {
  return time_us_32();
}
}

// Encoder update task - polls encoder hardware
void encoderUpdateTask(void *pvParameters) {
  auto *encoder = static_cast<RotaryEncoder *>(pvParameters);
  while (true) {
    encoder->update();
    vTaskDelay(pdMS_TO_TICKS(5)); // Poll every 5ms
  }
}

int main() {
  stdio_init_all();
  printf("1 - I2C Initialized\n");
  i2c_init(i2c1, 400'000);
  gpio_set_function(14, GPIO_FUNC_I2C);
  gpio_set_function(15, GPIO_FUNC_I2C);
  gpio_pull_up(14);
  gpio_pull_up(15);
  auto i2cbus = std::make_shared<PicoI2C>(1, 400'000);
  auto oLed = std::make_shared<ssd1306os>(i2cbus);
  // --- I2C setup for EEPROM ---
  printf("2 - Initializing I2C0 for EEPROM...\n");
  i2c_init(i2c0, 400'000);
  gpio_set_function(4, GPIO_FUNC_I2C);
  gpio_set_function(5, GPIO_FUNC_I2C);
  gpio_pull_up(4);
  gpio_pull_up(5);

  // --- Create mutexes ---
  printf("3 - Creating mutexes...\n");
  SemaphoreHandle_t eepromMutex = xSemaphoreCreateMutex();
  SemaphoreHandle_t sensorMutex = xSemaphoreCreateMutex();

  // --- Initialize Debug system ---
  printf("4 - Initializing Debug system...\n");
  auto debug = std::make_shared<Debug>();
  auto debugTask = std::make_shared<DebugTask>(debug);

  // --- Initialize EEPROM ---
  printf("5 - Initializing EEPROM...\n");
  auto eeprom = std::make_shared<Eeprom>(i2c0, 0x50, 2);

  // --- Initialize Credentials Manager ---
  printf("6 - Initializing Credentials Manager...\n");
  auto credentials = std::make_shared<SetCredentials>(*eeprom, eepromMutex);

  // --- Initialize SetpointManager ---
  printf("7 - Initializing SetpointManager...\n");
  auto setpointManager = std::make_shared<SetpointManager>(sensorMutex);

  // --- Initialize SensorHandler ---
  printf("8 - Initializing SensorHandler...\n");
  const auto sensorHandler = std::make_shared<SensorHandler>(
    setpointManager.get(),
    sensorMutex
  );

  // --- Initialize Rotary Encoder ---
  printf("9 - Initializing Rotary Encoder...\n");
  auto encoder = std::make_shared<RotaryEncoder>();

  // --- Initialize Input Manager (GPIO buttons 7, 8, 9) ---
  printf("10 - Initializing Input Manager...\n");
  auto inputManager = std::make_shared<InputManager>();

  // --- Initialize Cloud Handler ---
  printf("11 - Initializing Cloud Handler...\n");
  // auto cloudHandler = std::make_shared<CloudHandler>(setpointManager.get(), credentials.get());

  // --- Initialize Display Manager ---
  printf("12 - Initializing Display Manager...\n");
  // const auto displayManager = std::make_shared<DisplayManager>(debug);
  const auto displayManager = std::make_shared<DisplayManager>(debug, oLed);


  // --- Set Display Parameters ---
  printf("13 - Setting Display Parameters...\n");

  DisplayParams displayParams{
    .sensorHandler = sensorHandler.get(),
    .setpointManager = setpointManager.get(),
    .encoder = encoder.get(),
    .credentials = credentials.get(),
    .inputManager = inputManager.get(),
    .oLed = oLed.get()
  };
  displayManager->setParams(&displayParams);

  // --- Create FreeRTOS Tasks ---
  printf("14 - Creating FreeRTOS tasks...\n");

  // Task 1: Sensor Control (highest priority)
  xTaskCreate(
    SensorHandler::controlTask,
    "SensorControl",
    4096,
    sensorHandler.get(),
    3, // Priority 3 (highest)
    nullptr
  );
  printf("    - SensorControl task created\n");

  // Task 2: Display Update
  xTaskCreate(
    DisplayManager::taskEntry,
    "Display",
    8192,
    displayManager.get(),
    2, // Priority 2
    nullptr
  );
  printf("    - Display task created\n");

  // Task 3: Input Manager (button polling with GPIOPin)
  xTaskCreate(
    InputManager::taskEntry,
    "InputManager",
    2048,
    inputManager.get(),
    2, // Priority 2 (needs responsive input)
    nullptr
  );
  printf("    - InputManager task created\n");

  // Task 4: Encoder polling
  xTaskCreate(
    encoderUpdateTask,
    "EncoderUpdate",
    2048,
    encoder.get(),
    2, // Priority 2
    nullptr
  );
  printf("    - Encoder update task created\n");

  printf("15 - Starting FreeRTOS Scheduler...\n");
  printf("=====================================\n\n");

  vTaskStartScheduler();

  // Should never reach here
  while (true) {
    printf("ERROR: Scheduler exited!\n");
    sleep_ms(1000);
  }
}
