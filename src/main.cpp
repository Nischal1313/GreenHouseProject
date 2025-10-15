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
#include "inputManager.h"
#include "eeprom/eeprom.h"
#include "sensor_handler.h"
#include "setCredentials.h"
#include "cloud_handler.h"
#include "ssd1306os.h"

extern "C" {
uint32_t read_runtime_ctr(void) {
  return time_us_32();
}
}

// TODO move this into the encoder task that is implemented there.
[[noreturn]] void encoderUpdateTask(void *pvParameters) {
  auto *encoder = static_cast<RotaryEncoder *>(pvParameters);
  while (true) {
    encoder->update();
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}


[[noreturn]] int main() {
  stdio_init_all();

  i2c_init(i2c1, 400'000);
  gpio_set_function(14, GPIO_FUNC_I2C);
  gpio_set_function(15, GPIO_FUNC_I2C);
  gpio_pull_up(14);
  gpio_pull_up(15);

  auto i2cbus = std::make_shared<PicoI2C>(1, 400'000);
  auto oLed = std::make_shared<ssd1306os>(i2cbus);


  i2c_init(i2c0, 400'000);
  gpio_set_function(16, GPIO_FUNC_I2C);
  gpio_set_function(17, GPIO_FUNC_I2C);
  gpio_pull_up(16);
  gpio_pull_up(17);

  const auto eeprom = std::make_shared<Eeprom>(i2c0, 0x50, 2);


  SemaphoreHandle_t eepromMutex = xSemaphoreCreateMutex();
  SemaphoreHandle_t sensorMutex = xSemaphoreCreateMutex();


  auto debug = std::make_shared<Debug>();
  auto debugTask = std::make_shared<DebugTask>(debug);

  auto encoder = std::make_shared<RotaryEncoder>();

  const auto inputManager = std::make_shared<InputManager>();

  const auto credentials = std::make_shared<SetCredentials>(
    *eeprom, eepromMutex);

  auto sensorHandler = std::make_shared<SensorHandler>(
    sensorMutex, encoder,
    eepromMutex, *eeprom, inputManager);

  const auto cloudHandler = std::make_shared<CloudClass>(
    sensorHandler, eepromMutex, *eeprom);

  static DisplayManager displayManager(debug, oLed, encoder);

  DisplayParams displayParams{
    .sensorHandler = sensorHandler.get(),
    .credentials = credentials.get(),
    .inputManager = inputManager.get(),
    .oLed = oLed.get(),
    .encoder = encoder.get()
  };

  displayManager.setParams(&displayParams);


  xTaskCreate(encoderUpdateTask, "EncoderTask", 1024,
              encoder.get(), 3, nullptr);

  xTaskCreate(SensorHandler::controlTask, "SensorLogic", 2048,
              sensorHandler.get(), 2, nullptr);

  xTaskCreate(DisplayManager::taskEntry, "DisplayTask", 2048,
              &displayManager, 1, nullptr);

  xTaskCreate(InputManager::taskEntry, "InputHandler", 1024,
              inputManager.get(), 3, nullptr);

  xTaskCreate(CloudClass::taskEntry, "CloudHandler", 1024,
              cloudHandler.get(), 3, nullptr);

  vTaskStartScheduler();

  while (true) {
  }
}
