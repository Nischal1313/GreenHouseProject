#pragma once

#include <cstdint>
#include <cassert>
#include "FreeRTOS.h"
#include "task.h"

/// @brief Maximum number of concurrent states the supervisor tracks
static constexpr uint8_t MAX_STATE_COUNT = 16;

/// @brief All possible system states
enum class SystemState : uint8_t
{
    STARTUP          = 0,
    INIT_HARDWARE    = 1,
    LOAD_CONFIG      = 2,
    IDLE             = 3,
    MEASURE_SENSORS  = 4,
    CONTROL_OUTPUTS  = 5,
    UPDATE_DISPLAY   = 6,
    HANDLE_INPUT     = 7,
    CLOUD_SEND       = 8,
    CLOUD_RECEIVE    = 9,
    WIFI_CREDENTIALS = 10,
    ERROR_HALT       = 11,
    SAFE_STOP        = 12
};

/// @brief Context passed through all state handlers (static, never freed)
struct SystemContext
{
    // Hardware handles (set once during INIT_HARDWARE, never null after)
    class PicoI2C        *pI2cBusM;
    class ssd1306os      *pDisplayM;
    class Eeprom         *pEepromM;
    class Debug          *pDebugM;
    class RotaryEncoder  *pEncoderM;
    class InputManager   *pInputManagerM;
    class SensorHandler  *pSensorHandlerM;
    class CloudClass     *pCloudHandlerM;

    // State tracking
    uint32_t              errorCountM;
    uint32_t              cycleCountM;
    uint32_t              lastErrorTickM;
    SystemState           requestedStateM;

    // Fail-safe last-known-good values
    float                 lastGoodCo2M;
    float                 lastGoodTempM;
    float                 lastGoodHumidityM;
    float                 lastGoodFanSpeedM;
    int                   lastGoodSetpointM;
};

/// @brief A single state transition (guarded)
struct Transition
{
    SystemState targetStateM;
    bool      (*pGuardM)(SystemContext *pCtxP);
};

/// @brief Definition of one state: entry/do/exit handlers + transitions + timeout
struct StateDefinition
{
    SystemState (*pEntryM)(SystemContext *pCtxP);
    SystemState (*pDoActivityM)(SystemContext *pCtxP);
    SystemState (*pExitM)(SystemContext *pCtxP);
    Transition  const *pTransitionsM;
    uint8_t            transitionCountM;
    TickType_t         timeoutTicksM;
};

/// @brief Safety-critical state machine supervisor.
///        Drives all states, enforces timeouts, validates transitions,
///        and routes errors to ERROR_HALT / SAFE_STOP.
class SystemStateMachine
{
public:
    SystemStateMachine(StateDefinition const *pStatesP,
                       SystemContext *pCtxP,
                       TickType_t tickIntervalMsP);

    /// @brief Execute one cycle of the current state.
    /// @return Current state after the cycle (may have transitioned).
    SystemState executeCycle();

    /// @brief Force a state transition (checked).
    void requestState(SystemState stateP);

    [[nodiscard]] SystemState getCurrentState() const
    {
        return currentStateM;
    }

    [[nodiscard]] SystemContext *getContext()
    {
        return pCtxM;
    }

    [[nodiscard]] uint32_t getErrorCount() const
    {
        return pCtxM->errorCountM;
    }

    [[nodiscard]] bool isInSafeState() const
    {
        return currentStateM == SystemState::SAFE_STOP
            || currentStateM == SystemState::ERROR_HALT;
    }

private:
    bool isValidTransition(SystemState fromP, SystemState toP) const;
    SystemState onTimeout(SystemState stateP);
    SystemState transitionTo(SystemState nextP);

    StateDefinition const *pStatesM;
    SystemContext         *pCtxM;
    SystemState            currentStateM;
    SystemState            previousStateM;
    TickType_t             tickIntervalMsM;
    bool                   stateEntryDoneM;
    TickType_t             stateStartTickM;
    static uint8_t const   STATE_COUNT = 13;
};

// ============================================================================
// Guard functions (shared by multiple transitions)
// ============================================================================

bool guardSensorDataReady(SystemContext *pCtxP);
bool guardEncoderTurned(SystemContext *pCtxP);
bool guardButtonPressed(SystemContext *pCtxP);
bool guardWifiConnected(SystemContext *pCtxP);
bool guardCloudSendDue(SystemContext *pCtxP);
bool guardCloudReceiveDue(SystemContext *pCtxP);
bool guardWifiMenuActive(SystemContext *pCtxP);
bool guardNoError(SystemContext *pCtxP);
bool guardErrorOccurred(SystemContext *pCtxP);
