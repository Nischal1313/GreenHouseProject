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
#include "state_machine.h"
#include "system_state.h"

// ============================================================================
// Hardware constants (compile-time, never changed at runtime)
// ============================================================================
namespace
{
    constexpr uint    I2C0_SDA_PIN{16};
    constexpr uint    I2C0_SCL_PIN{17};
    constexpr uint    I2C1_SDA_PIN{14};
    constexpr uint    I2C1_SCL_PIN{15};
    constexpr uint    I2C_SPEED{400'000};
    constexpr uint8_t EEPROM_I2C_ADDR{0x50};
    constexpr uint8_t EEPROM_ADDR_WIDTH{2};
    constexpr TickType_t STATE_MACHINE_TICK_MS{100};
    constexpr uint       SUPERVISOR_STACK_SIZE{2048};
    constexpr UBaseType_t SUPERVISOR_PRIORITY{2};
}

// ============================================================================
// Globals (pre-allocated, never freed)
// ============================================================================
extern "C"
{
    uint32_t read_runtime_ctr(void)
    {
        return time_us_32();
    }
}

namespace
{
    // All system objects — statically allocated via shared_ptr,
    // created once in init, never destroyed
    std::shared_ptr<PicoI2C>       pI2cBusS;
    std::shared_ptr<ssd1306os>     pDisplayS;
    std::shared_ptr<Eeprom>        pEepromS;
    std::shared_ptr<Debug>         pDebugS;
    std::shared_ptr<DebugTask>     pDebugTaskS;
    std::shared_ptr<RotaryEncoder> pEncoderS;
    std::shared_ptr<InputManager>  pInputManagerS;
    std::shared_ptr<SetCredentials> pCredentialsS;
    std::shared_ptr<SensorHandler> pSensorHandlerS;
    std::shared_ptr<CloudClass>    pCloudHandlerS;

    // Display manager (static to avoid shared_ptr cycle)
    DisplayManager    *pDisplayManagerS{nullptr};

    // System context and state machine
    SystemContext      systemContextS{};
    SystemStateMachine *pStateMachineS{nullptr};
}

// ============================================================================
// State machine supervisor task (single control loop)
// ============================================================================
[[noreturn]] void supervisorTask(void *pvParametersP)
{
    (void)pvParametersP;
    SystemStateMachine *pSm = pStateMachineS;

    printf("[SUPERVISOR] State machine started\n");

    while (true)
    {
        // Execute one state machine cycle
        SystemState currentState = pSm->executeCycle();

        // Render display (non-blocking — just writes to OLED buffer)
        if (pDisplayManagerS != nullptr)
        {
            bool const inMainMenu = pInputManagerS != nullptr
                && pInputManagerS->isMainMenu();

            if (inMainMenu)
            {
                pDisplayManagerS->drawMainMenu();
            }
            else
            {
                pDisplayManagerS->handleWifiMenuButtons();
                pDisplayManagerS->drawWifiMenu();
            }
        }

        // If we've reached safe stop, halt here
        if (currentState == SystemState::SAFE_STOP)
        {
            printf("[SUPERVISOR] SAFE_STOP — system halted\n");
            vTaskSuspend(nullptr);
        }

        // Fixed-rate tick — yields CPU to idle task
        vTaskDelay(pdMS_TO_TICKS(STATE_MACHINE_TICK_MS));
    }
}

// ============================================================================
// Main entry — init hardware, create objects, start scheduler
// ============================================================================
[[noreturn]] int main()
{
    stdio_init_all();

    // ---- Phase 1: Hardware init ----
    i2c_init(i2c1, I2C_SPEED);
    gpio_set_function(I2C1_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C1_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C1_SDA_PIN);
    gpio_pull_up(I2C1_SCL_PIN);

    auto pI2cBus = std::make_shared<PicoI2C>(1, I2C_SPEED);
    auto pDisplay = std::make_shared<ssd1306os>(pI2cBus);

    i2c_init(i2c0, I2C_SPEED);
    gpio_set_function(I2C0_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C0_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C0_SDA_PIN);
    gpio_pull_up(I2C0_SCL_PIN);

    auto pEeprom = std::make_shared<Eeprom>(i2c0, EEPROM_I2C_ADDR, EEPROM_ADDR_WIDTH);

    // ---- Phase 2: Mutex init ----
    SemaphoreHandle_t eepromMutex = xSemaphoreCreateMutex();
    SemaphoreHandle_t sensorMutex = xSemaphoreCreateMutex();

    // ---- Phase 3: Object construction (no allocation after this) ----
    auto pDebug = std::make_shared<Debug>();
    auto pDebugTask = std::make_shared<DebugTask>(pDebug);

    auto pEncoder = std::make_shared<RotaryEncoder>();
    auto const pInputManager = std::make_shared<InputManager>();

    auto const pCredentials = std::make_shared<SetCredentials>(
        *pEeprom, eepromMutex);

    auto pSensorHandler = std::make_shared<SensorHandler>(
        sensorMutex, pEncoder,
        eepromMutex, *pEeprom, pInputManager);

    auto const pCloudHandler = std::make_shared<CloudClass>(
        pSensorHandler, eepromMutex, *pEeprom);

    static DisplayManager displayManager(pDebug, pDisplay, pEncoder);

    DisplayParams displayParams{
        .pSensorHandlerM = pSensorHandler.get(),
        .pCredentialsM   = pCredentials.get(),
        .pInputManagerM  = pInputManager.get(),
        .pOLedM          = pDisplay.get(),
        .pEncoderM       = pEncoder.get()
    };

    displayManager.setParams(&displayParams);

    // ---- Phase 4: Populate globals for supervisor task ----
    pI2cBusS       = pI2cBus;
    pDisplayS      = pDisplay;
    pEepromS       = pEeprom;
    pDebugS        = pDebug;
    pDebugTaskS    = pDebugTask;
    pEncoderS      = pEncoder;
    pInputManagerS = pInputManager;
    pCredentialsS  = pCredentials;
    pSensorHandlerS = pSensorHandler;
    pCloudHandlerS  = pCloudHandler;
    pDisplayManagerS = &displayManager;

    // ---- Phase 5: Populate system context ----
    systemContextS.pI2cBusM         = pI2cBus.get();
    systemContextS.pDisplayM        = pDisplay.get();
    systemContextS.pEepromM         = pEeprom.get();
    systemContextS.pDebugM          = pDebug.get();
    systemContextS.pEncoderM        = pEncoder.get();
    systemContextS.pInputManagerM   = pInputManager.get();
    systemContextS.pSensorHandlerM  = pSensorHandler.get();
    systemContextS.pCloudHandlerM   = pCloudHandler.get();
    systemContextS.errorCountM      = 0;
    systemContextS.cycleCountM      = 0;
    systemContextS.lastErrorTickM   = 0;
    systemContextS.requestedStateM  = SystemState::STARTUP;
    systemContextS.lastGoodCo2M     = 0.0f;
    systemContextS.lastGoodTempM    = 0.0f;
    systemContextS.lastGoodHumidityM = 0.0f;
    systemContextS.lastGoodFanSpeedM = 0.0f;
    systemContextS.lastGoodSetpointM = 400;

    // Create state machine (uses stateTableM from system_state.cpp)
    static SystemStateMachine stateMachine(
        stateTableM,
        &systemContextS,
        STATE_MACHINE_TICK_MS);

    pStateMachineS = &stateMachine;

    // ---- Phase 6: Create FreeRTOS tasks ----
    // Encoder: fast polling required (5ms)
    xTaskCreate(RotaryEncoder::taskEntry, "EncoderTask", 1024,
                pEncoder.get(), 3, nullptr);

    // Input buttons: moderate polling
    xTaskCreate(InputManager::taskEntry, "InputHandler", 1024,
                pInputManager.get(), 3, nullptr);

    // Supervisor: single state-machine-driven control loop
    xTaskCreate(supervisorTask, "Supervisor", SUPERVISOR_STACK_SIZE,
                nullptr, SUPERVISOR_PRIORITY, nullptr);

    // ---- Phase 7: Start FreeRTOS scheduler ----
    vTaskStartScheduler();

    // Should never reach here
    while (true)
    {
    }
}
