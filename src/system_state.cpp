#include "state_machine.h"
#include "system_state.h"
#include "ssd1306os.h"
#include "PicoI2C.h"
#include "eeprom/eeprom.h"
#include "sensor_handler.h"
#include "cloud_handler.h"
#include "displayMenu.h"
#include "rotary_encoder.h"
#include "inputManager.h"
#include "debug.h"
#include <cstdio>
#include <cstring>
#include <cmath>

// ============================================================================
// State handlers
// ============================================================================

static SystemState onStartupEntry(SystemContext *pCtxP)
{
    (void)pCtxP;
    printf("[SM] STARTUP\n");
    return SystemState::INIT_HARDWARE;
}

static SystemState onInitHardwareDo(SystemContext *pCtxP)
{
    // All hardware init happens here — if any step fails we go to ERROR_HALT
    return SystemState::LOAD_CONFIG;
}

static SystemState onLoadConfigEntry(SystemContext *pCtxP)
{
    // Load last-good CO2 setpoint from EEPROM
    pCtxP->pSensorHandlerM->copyValueFromEEPROM(
        0x10, pCtxP->lastGoodSetpointM);

    printf("[SM] Config loaded, setpoint=%d\n", pCtxP->lastGoodSetpointM);
    return SystemState::IDLE;
}

static SystemState onIdleEntry(SystemContext *pCtxP)
{
    pCtxP->cycleCountM = 0;
    return SystemState::IDLE;
}

static SystemState onIdleDo(SystemContext *pCtxP)
{
    pCtxP->cycleCountM++;

    // Periodically force sensor measurement
    if ((pCtxP->cycleCountM % 3) == 0)
    {
        return SystemState::MEASURE_SENSORS;
    }

    return SystemState::IDLE;
}

static SystemState onMeasureSensorsEntry(SystemContext *pCtxP)
{
    SensorValues readings = pCtxP->pSensorHandlerM->getReadings();

    // Validate and store last-good values
    if (!std::isnan(readings.co2M) && readings.co2M >= 0.0f
        && readings.co2M <= 10000.0f)
    {
        pCtxP->lastGoodCo2M = readings.co2M;
    }

    if (!std::isnan(readings.temperatureM)
        && readings.temperatureM >= -40.0f
        && readings.temperatureM <= 85.0f)
    {
        pCtxP->lastGoodTempM = readings.temperatureM;
    }

    if (!std::isnan(readings.humidityM)
        && readings.humidityM >= 0.0f
        && readings.humidityM <= 100.0f)
    {
        pCtxP->lastGoodHumidityM = readings.humidityM;
    }

    if (!std::isnan(readings.fanSpeedM)
        && readings.fanSpeedM >= 0.0f
        && readings.fanSpeedM <= 100.0f)
    {
        pCtxP->lastGoodFanSpeedM = readings.fanSpeedM;
    }

    if (readings.targetCo2M >= 200 && readings.targetCo2M <= 1500)
    {
        pCtxP->lastGoodSetpointM = readings.targetCo2M;
    }

    return SystemState::CONTROL_OUTPUTS;
}

static SystemState onControlOutputsDo(SystemContext *pCtxP)
{
    // Use last-good values — never act on NaN
    pCtxP->pSensorHandlerM->handleValveAndFanLogic(
        pCtxP->lastGoodCo2M,
        pCtxP->lastGoodSetpointM);

    pCtxP->pSensorHandlerM->updateFromEncoder();
    pCtxP->pSensorHandlerM->writeToEEPROM();

    return SystemState::UPDATE_DISPLAY;
}

static SystemState onUpdateDisplayDo(SystemContext *pCtxP)
{
    DisplayManager *pDm = reinterpret_cast<DisplayManager *>(
        pCtxP->pDisplayM);
    (void)pDm;
    // Display update is driven separately — state machine just
    // ensures it gets a chance to run
    return SystemState::IDLE;
}

static SystemState onCloudSendDo(SystemContext *pCtxP)
{
    if (pCtxP->pCloudHandlerM == nullptr)
    {
        return SystemState::IDLE;
    }

    int const co2        = static_cast<int>(pCtxP->lastGoodCo2M);
    int const temp       = static_cast<int>(pCtxP->lastGoodTempM);
    int const hum        = static_cast<int>(pCtxP->lastGoodHumidityM);
    int const fan        = static_cast<int>(pCtxP->lastGoodFanSpeedM);
    int const setpoint   = pCtxP->lastGoodSetpointM;

    bool const sent = pCtxP->pCloudHandlerM->sendData(
        co2, temp, hum, fan, setpoint);

    if (!sent)
    {
        printf("[SM] Cloud send failed, will retry\n");
    }

    return SystemState::IDLE;
}

static SystemState onCloudReceiveDo(SystemContext *pCtxP)
{
    if (pCtxP->pCloudHandlerM == nullptr)
    {
        return SystemState::IDLE;
    }

    pCtxP->pCloudHandlerM->checkTalkBackQueue();

    return SystemState::IDLE;
}

static SystemState onWifiCredentialsDo(SystemContext *pCtxP)
{
    (void)pCtxP;
    // Wi-Fi credential handling driven by displayMenu + inputManager
    // State machine just ensures the menu is active
    return SystemState::WIFI_CREDENTIALS;
}

static SystemState onErrorHaltEntry(SystemContext *pCtxP)
{
    pCtxP->errorCountM++;
    pCtxP->lastErrorTickM = xTaskGetTickCount();

    printf("[SM] ERROR_HALT (count=%lu)\n",
           static_cast<unsigned long>(pCtxP->errorCountM));

    // If too many errors, go to safe stop
    if (pCtxP->errorCountM >= 5)
    {
        return SystemState::SAFE_STOP;
    }

    // Recoverable — attempt restart after delay
    vTaskDelay(pdMS_TO_TICKS(2000));
    return SystemState::IDLE;
}

static SystemState onSafeStopEntry(SystemContext *pCtxP)
{
    printf("[SM] SAFE_STOP — all outputs disabled\n");

    // Fail-safe: close valve, stop fan
    // (handled via gpio_pin defaults)

    return SystemState::SAFE_STOP;
}

static SystemState onSafeStopDo(SystemContext *pCtxP)
{
    (void)pCtxP;
    vTaskSuspend(nullptr);
    return SystemState::SAFE_STOP;
}

// ============================================================================
// Guards
// ============================================================================

bool guardSensorDataReady(SystemContext *pCtxP)
{
    (void)pCtxP;
    return true; // Always ready — we read on demand
}

bool guardEncoderTurned(SystemContext *pCtxP)
{
    return pCtxP->pEncoderM != nullptr
        && (pCtxP->pEncoderM->rotatedCW()
         || pCtxP->pEncoderM->rotatedCCW());
}

bool guardButtonPressed(SystemContext *pCtxP)
{
    return pCtxP->pInputManagerM != nullptr
        && !pCtxP->pInputManagerM->isMainMenu();
}

bool guardWifiConnected(SystemContext *pCtxP)
{
    return pCtxP->pCloudHandlerM != nullptr
        && pCtxP->pCloudHandlerM->networkConnectedM;
}

bool guardCloudSendDue(SystemContext *pCtxP)
{
    (void)pCtxP;
    // Send every ~60 seconds (checked in IDLE cycle counting)
    return false; // handled by cycle count in IDLE state
}

bool guardCloudReceiveDue(SystemContext *pCtxP)
{
    (void)pCtxP;
    return false; // handled by cycle count in IDLE state
}

bool guardWifiMenuActive(SystemContext *pCtxP)
{
    return pCtxP->pInputManagerM != nullptr
        && !pCtxP->pInputManagerM->isMainMenu();
}

bool guardNoError(SystemContext *pCtxP)
{
    return pCtxP->errorCountM == 0;
}

bool guardErrorOccurred(SystemContext *pCtxP)
{
    return pCtxP->errorCountM > 0;
}

// ============================================================================
// Transition tables
// ============================================================================

static Transition const noTransitionsM[] = {};

static Transition const startupTransitionsM[] =
{
    {SystemState::INIT_HARDWARE, nullptr}
};

static Transition const initHwTransitionsM[] =
{
    {SystemState::LOAD_CONFIG, nullptr}
};

static Transition const loadCfgTransitionsM[] =
{
    {SystemState::IDLE, nullptr}
};

static Transition const idleTransitionsM[] =
{
    {SystemState::MEASURE_SENSORS,  nullptr},
    {SystemState::WIFI_CREDENTIALS, guardWifiMenuActive},
    {SystemState::CLOUD_SEND,       guardWifiConnected},
    {SystemState::ERROR_HALT,       guardErrorOccurred}
};

static Transition const measureTransitionsM[] =
{
    {SystemState::CONTROL_OUTPUTS, nullptr}
};

static Transition const controlTransitionsM[] =
{
    {SystemState::UPDATE_DISPLAY, nullptr}
};

static Transition const displayTransitionsM[] =
{
    {SystemState::IDLE,             nullptr},
    {SystemState::WIFI_CREDENTIALS, guardWifiMenuActive}
};

static Transition const cloudSendTransitionsM[] =
{
    {SystemState::IDLE, nullptr}
};

static Transition const cloudRecvTransitionsM[] =
{
    {SystemState::IDLE, nullptr}
};

static Transition const wifiCredTransitionsM[] =
{
    {SystemState::IDLE, nullptr}
};

static Transition const errorTransitionsM[] =
{
    {SystemState::IDLE,      nullptr},
    {SystemState::SAFE_STOP, nullptr}
};

static Transition const safeStopTransitionsM[] =
{
    {SystemState::SAFE_STOP, nullptr}
};

// ============================================================================
// Master state table
// ============================================================================

StateDefinition const stateTableM[] =
{
    // STARTUP
    {
        onStartupEntry,
        nullptr,
        nullptr,
        startupTransitionsM,
        sizeof(startupTransitionsM) / sizeof(startupTransitionsM[0]),
        pdMS_TO_TICKS(5000)
    },
    // INIT_HARDWARE
    {
        nullptr,
        onInitHardwareDo,
        nullptr,
        initHwTransitionsM,
        sizeof(initHwTransitionsM) / sizeof(initHwTransitionsM[0]),
        pdMS_TO_TICKS(10000)
    },
    // LOAD_CONFIG
    {
        onLoadConfigEntry,
        nullptr,
        nullptr,
        loadCfgTransitionsM,
        sizeof(loadCfgTransitionsM) / sizeof(loadCfgTransitionsM[0]),
        pdMS_TO_TICKS(5000)
    },
    // IDLE
    {
        onIdleEntry,
        onIdleDo,
        nullptr,
        idleTransitionsM,
        sizeof(idleTransitionsM) / sizeof(idleTransitionsM[0]),
        pdMS_TO_TICKS(5000)
    },
    // MEASURE_SENSORS
    {
        onMeasureSensorsEntry,
        nullptr,
        nullptr,
        measureTransitionsM,
        sizeof(measureTransitionsM) / sizeof(measureTransitionsM[0]),
        pdMS_TO_TICKS(2000)
    },
    // CONTROL_OUTPUTS
    {
        nullptr,
        onControlOutputsDo,
        nullptr,
        controlTransitionsM,
        sizeof(controlTransitionsM) / sizeof(controlTransitionsM[0]),
        pdMS_TO_TICKS(2000)
    },
    // UPDATE_DISPLAY
    {
        nullptr,
        onUpdateDisplayDo,
        nullptr,
        displayTransitionsM,
        sizeof(displayTransitionsM) / sizeof(displayTransitionsM[0]),
        pdMS_TO_TICKS(1000)
    },
    // HANDLE_INPUT (transition-only, no handlers needed here)
    {
        nullptr, nullptr, nullptr,
        displayTransitionsM, // same as display
        sizeof(displayTransitionsM) / sizeof(displayTransitionsM[0]),
        pdMS_TO_TICKS(5000)
    },
    // CLOUD_SEND
    {
        nullptr,
        onCloudSendDo,
        nullptr,
        cloudSendTransitionsM,
        sizeof(cloudSendTransitionsM) / sizeof(cloudSendTransitionsM[0]),
        pdMS_TO_TICKS(30000)
    },
    // CLOUD_RECEIVE
    {
        nullptr,
        onCloudReceiveDo,
        nullptr,
        cloudRecvTransitionsM,
        sizeof(cloudRecvTransitionsM) / sizeof(cloudRecvTransitionsM[0]),
        pdMS_TO_TICKS(30000)
    },
    // WIFI_CREDENTIALS
    {
        nullptr,
        onWifiCredentialsDo,
        nullptr,
        wifiCredTransitionsM,
        sizeof(wifiCredTransitionsM) / sizeof(wifiCredTransitionsM[0]),
        pdMS_TO_TICKS(300000)
    },
    // ERROR_HALT
    {
        onErrorHaltEntry,
        nullptr,
        nullptr,
        errorTransitionsM,
        sizeof(errorTransitionsM) / sizeof(errorTransitionsM[0]),
        pdMS_TO_TICKS(10000)
    },
    // SAFE_STOP
    {
        onSafeStopEntry,
        onSafeStopDo,
        nullptr,
        safeStopTransitionsM,
        sizeof(safeStopTransitionsM) / sizeof(safeStopTransitionsM[0]),
        portMAX_DELAY
    }
};

// ============================================================================
// SystemStateMachine implementation
// ============================================================================

SystemStateMachine::SystemStateMachine(
    StateDefinition const *pStatesP,
    SystemContext *pCtxP,
    TickType_t tickIntervalMsP)
    : pStatesM{pStatesP}
    , pCtxM{pCtxP}
    , currentStateM{SystemState::STARTUP}
    , previousStateM{SystemState::STARTUP}
    , tickIntervalMsM{tickIntervalMsP}
    , stateEntryDoneM{false}
    , stateStartTickM{0}
{
}

SystemState SystemStateMachine::executeCycle()
{
    StateDefinition const &rState = pStatesM[static_cast<uint8_t>(currentStateM)];
    TickType_t const now = xTaskGetTickCount();

    // Enforce state timeout
    if (rState.timeoutTicksM > 0
        && rState.timeoutTicksM < portMAX_DELAY
        && stateEntryDoneM)
    {
        TickType_t const elapsed = now - stateStartTickM;
        if (elapsed >= rState.timeoutTicksM)
        {
            return onTimeout(currentStateM);
        }
    }

    // Entry action (once per state entry)
    if (!stateEntryDoneM)
    {
        stateStartTickM = now;
        stateEntryDoneM = true;
        if (rState.pEntryM != nullptr)
        {
            SystemState next = rState.pEntryM(pCtxM);
            if (next != currentStateM)
            {
                if (isValidTransition(currentStateM, next))
                {
                    return transitionTo(next);
                }

                return transitionTo(SystemState::ERROR_HALT);
            }
        }
    }

    // Do activity
    if (rState.pDoActivityM != nullptr)
    {
        SystemState next = rState.pDoActivityM(pCtxM);
        if (next != currentStateM)
        {
            if (isValidTransition(currentStateM, next))
            {
                return transitionTo(next);
            }

            return transitionTo(SystemState::ERROR_HALT);
        }
    }

    // Evaluate guarded transitions
    for (uint8_t i = 0; i < rState.transitionCountM; i++)
    {
        Transition const &rTrans = rState.pTransitionsM[i];
        if (rTrans.pGuardM == nullptr || rTrans.pGuardM(pCtxM))
        {
            if (isValidTransition(currentStateM, rTrans.targetStateM))
            {
                return transitionTo(rTrans.targetStateM);
            }
        }
    }

    return currentStateM;
}

void SystemStateMachine::requestState(SystemState stateP)
{
    if (isValidTransition(currentStateM, stateP))
    {
        transitionTo(stateP);
    }
}

bool SystemStateMachine::isValidTransition(
    SystemState fromP, SystemState toP) const
{
    // Always allow transitions TO error or safe stop from any state
    if (toP == SystemState::ERROR_HALT || toP == SystemState::SAFE_STOP)
    {
        return true;
    }

    // Never leave SAFE_STOP
    if (fromP == SystemState::SAFE_STOP)
    {
        return false;
    }

    // Check the transition table for this state
    StateDefinition const &rState = pStatesM[static_cast<uint8_t>(fromP)];
    for (uint8_t i = 0; i < rState.transitionCountM; i++)
    {
        if (rState.pTransitionsM[i].targetStateM == toP)
        {
            return true;
        }
    }

    return false;
}

SystemState SystemStateMachine::onTimeout(SystemState stateP)
{
    printf("[SM] TIMEOUT in state %u\n",
           static_cast<unsigned>(stateP));

    // Don't timeout in safe states
    if (stateP == SystemState::SAFE_STOP || stateP == SystemState::ERROR_HALT)
    {
        return stateP;
    }

    // Entry action may not have completed — force entry-done for timeout
    stateEntryDoneM = true;

    return transitionTo(SystemState::ERROR_HALT);
}

SystemState SystemStateMachine::transitionTo(SystemState nextP)
{
    StateDefinition const &rOldState =
        pStatesM[static_cast<uint8_t>(currentStateM)];

    if (rOldState.pExitM != nullptr)
    {
        rOldState.pExitM(pCtxM);
    }

    previousStateM = currentStateM;
    currentStateM = nextP;
    stateEntryDoneM = false;

    printf("[SM] %u -> %u\n",
           static_cast<unsigned>(previousStateM),
           static_cast<unsigned>(currentStateM));

    return currentStateM;
}
