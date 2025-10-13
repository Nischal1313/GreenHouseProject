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

// Encoder update task - polls encoder hardware every 5ms
[[noreturn]] void encoderUpdateTask(void *pvParameters) {
  auto *encoder = static_cast<RotaryEncoder *>(pvParameters);
  printf("[EncoderTask] Started\n");
  while (true) {
    encoder->update();
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

// Setpoint update task - reads encoder events ONLY when in MAIN menu
[[noreturn]] void setpointUpdateTask(void *pvParameters) {
  auto **params = static_cast<void**>(pvParameters);
  auto *setpointManager = static_cast<SetpointManager*>(params[0]);
  auto *displayManager = static_cast<DisplayManager*>(params[1]);

  printf("[SetpointTask] Started\n");
  vTaskDelay(pdMS_TO_TICKS(100));
}


int main() {
  stdio_init_all();
  printf("\n\n========================================\n");
  printf("CO2 Control System Starting...\n");
  printf("========================================\n");

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
  gpio_set_function(4, GPIO_FUNC_I2C);
  gpio_set_function(5, GPIO_FUNC_I2C);
  gpio_pull_up(4);
  gpio_pull_up(5);
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
  printf("   Encoder initialized (pins A=10, B=11, Button=12)\n");

  // --- Initialize Input Manager (GPIO buttons) ---
  printf("6 - Initializing Input Manager...\n");
  auto inputManager = std::make_shared<InputManager>();

  // --- Initialize Credentials Manager ---
  printf("7 - Initializing Credentials Manager...\n");
  auto credentials = std::make_shared<SetCredentials>(*eeprom, eepromMutex);

  // --- Initialize SetpointManager (shares encoder) ---
  printf("8 - Initializing SetpointManager...\n");
  auto setpointManager = std::make_shared<SetpointManager>(sensorMutex, eeprom, encoder);

  // --- Initialize SensorHandler ---
  printf("9 - Initializing SensorHandler...\n");
  auto sensorHandler = std::make_shared<SensorHandler>(
    setpointManager.get(),
    sensorMutex
  );
  printf("   Sensor handler initialized\n");

  // --- Initialize Display Manager ---
  // printf("10 - Initializing Display Manager...\n");
  // auto displayManager = std::make_shared<DisplayManager>(debug, oLed, encoder);
  //
  // // --- Set Display Parameters ---
  // printf("11 - Setting Display Parameters...\n");
  // DisplayParams displayParams{
  //   .sensorHandler = sensorHandler.get(),
  //   .setpointManager = setpointManager.get(),
  //   .credentials = credentials.get(),
  //   .inputManager = inputManager.get(),
  //   .oLed = oLed.get(),
  //   .encoder = encoder.get()
  // };
  // displayManager->setParams(&displayParams);

  // --- Create FreeRTOS Tasks ---
  printf("12 - Creating FreeRTOS tasks...\n");

  // Task 1: Encoder hardware polling (highest priority for responsiveness)
  xTaskCreate(
    encoderUpdateTask,
    "EncoderHW",
    2048,
    encoder.get(),
    3, // Priority 3 - needs to be responsive
    nullptr
  );
  printf("    - Encoder hardware task created\n");

  // Task 2: Setpoint management (reads encoder events)
  xTaskCreate(
    setpointUpdateTask,
    "SetpointMgr",
    2048,
    setpointManager.get(),
    2, // Priority 2
    nullptr
  );
  printf("    - Setpoint manager task created\n");

  // Task 3: Sensor Control
  xTaskCreate(
    SensorHandler::controlTask,
    "SensorCtrl",
    4096,
    sensorHandler.get(),
    2, // Priority 2
    nullptr
  );
  printf("    - Sensor control task created\n");

  // // Task 4: Display Update
  // xTaskCreate(
  //   DisplayManager::taskEntry,
  //   "Display",
  //   8192,
  //   displayManager.get(),
  //   1, // Priority 1 (lower - not time critical)
  //   nullptr
  // );
  // printf("    - Display task created\n");

  // Task 5: Input Manager (button polling)
  xTaskCreate(
    InputManager::taskEntry,
    "InputMgr",
    2048,
    inputManager.get(),
    2, // Priority 2
    nullptr
  );
  printf("    - Input manager task created\n");

  printf("\n========================================\n");
  printf("Starting FreeRTOS Scheduler...\n");
  printf("========================================\n\n");

  vTaskStartScheduler();

  // Should never reach here
  printf("ERROR: Scheduler exited!\n");
  while (true) {
    tight_loop_contents();
  }
}