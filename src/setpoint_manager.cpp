#include "setpoint_manager.h"
#include "pico/time.h"
#include <algorithm>
#include <cstdio>

// --- constants ---
constexpr uint16_t MIN_CO2 = 200;
constexpr uint16_t MAX_CO2 = 1500;
constexpr uint32_t EEPROM_SAVE_INTERVAL_MS = 10000; // 10 seconds

SetpointManager::SetpointManager(const SemaphoreHandle_t mutex,
                                 const std::shared_ptr<Eeprom> &eepromPtr,
                                 const std::shared_ptr<RotaryEncoder> &encoderPtr)
  : mutex(mutex),
    eeprom(eepromPtr),
    encoder(encoderPtr),
    localValue(400),  // Start with default
    eepromValue(0),
    lastSaveTick(xTaskGetTickCount()) {

  printf("[SetpointManager] Initializing with encoder=%p\n", static_cast<void *>(encoder.get()));

  // Read initial value from EEPROM
  uint8_t buf[2];
  int bytesRead = eeprom->readBlock(co2Address, buf, 2);

  if (bytesRead == 2) {
    // Reconstruct 16-bit value from two bytes
    eepromValue = (buf[0] << 8) | buf[1];

    // Validate the EEPROM value is in valid range
    if (eepromValue >= MIN_CO2 && eepromValue <= MAX_CO2) {
      localValue = eepromValue;
      printf("[SetpointManager] Loaded CO2 setpoint from EEPROM: %u ppm\n", eepromValue);
    } else {
      // Invalid value in EEPROM, use default
      localValue = 400;
      eepromValue = localValue;
      printf("[SetpointManager] Invalid EEPROM value (%u), using default: %u ppm\n",
             eepromValue, localValue);
      writeToEEPROM();
    }
  } else {
    // EEPROM read failed, use default
    localValue = 400;
    eepromValue = localValue;
    printf("[SetpointManager] EEPROM read failed, using default: %u ppm\n", localValue);
    writeToEEPROM();
  }
}

void SetpointManager::updateLocal(const int value, const MenuState activeMenu) {
  if (activeMenu != MenuState::MAIN) return;

  MutexGuard lock(mutex);
  if (!lock.owns_lock()) {
    printf("[SetpointManager] Failed to acquire mutex in updateLocal\n");
    return;
  }

  const uint oldValue = localValue;
  localValue = std::clamp(value, static_cast<int>(MIN_CO2), static_cast<int>(MAX_CO2));

  if (localValue != oldValue) {
    printf("[SetpointManager] Local setpoint updated: %u -> %u ppm\n", oldValue, localValue);
    notifyCallbacks(localValue);
  }
}

int SetpointManager::getEffectiveTarget() const {
  MutexGuard lock(mutex);
  if (!lock.owns_lock()) {
    printf("[SetpointManager] WARNING: Could not acquire mutex in getEffectiveTarget!\n");
    return localValue;
  }
  return localValue;
}

void SetpointManager::subscribeToSetpointChanges(const std::function<void(int)> &cb) {
  MutexGuard lock(mutex);
  if (!lock.owns_lock()) return;
  callbacks_.push_back(cb);
}

void SetpointManager::notifyCallbacks(const int newValue) const {
  for (auto &cb: callbacks_) {
    if (cb) cb(newValue);
  }
}

void SetpointManager::writeToEEPROM() {
  const uint8_t buf[2] = {
    static_cast<uint8_t>(localValue >> 8),
    static_cast<uint8_t>(localValue & 0xFF)
  };
  eeprom->writeBlock(co2Address, buf, 2);
  eepromValue = localValue;
  printf("[SetpointManager] Saved CO2 setpoint to EEPROM: %u ppm\n", localValue);
}

void SetpointManager::updateFromEncoder(const MenuState currentMenu) {
  // ONLY process encoder events when in MAIN menu
  if (currentMenu != MenuState::MAIN) {
    // Not in MAIN menu - don't consume encoder events (they're for WiFi menu)
    return;
  }

  if (!encoder) {
    printf("[SetpointManager] ERROR: encoder is null!\n");
    return;
  }

  // Check for encoder rotation events in MAIN menu
  bool cwRotation = encoder->rotatedCW();
  bool ccwRotation = encoder->rotatedCCW();

  if (!cwRotation && !ccwRotation) {
    // No rotation - check if we need to save to EEPROM
    TickType_t currentTick = xTaskGetTickCount();
    if (localValue != eepromValue && (currentTick - lastSaveTick >= saveIntervalTicks)) {
      MutexGuard lock(mutex);
      if (lock.owns_lock()) {
        writeToEEPROM();
        lastSaveTick = currentTick;
      }
    }
    return;
  }

  // Rotation detected in MAIN menu - update setpoint value
  MutexGuard lock(mutex);
  if (!lock.owns_lock()) {
    printf("[SetpointManager] WARNING: Could not acquire mutex for encoder update!\n");
    return;
  }

  const uint oldValue = localValue;

  if (cwRotation) {
    localValue = std::min(localValue + 10, static_cast<uint>(MAX_CO2));
    printf("[SetpointManager] Encoder CW in MAIN: %u -> %u ppm\n", oldValue, localValue);
  } if (ccwRotation) {
    localValue = std::max(localValue - 10, static_cast<uint>(MIN_CO2));
    printf("[SetpointManager] Encoder CCW in MAIN: %u -> %u ppm\n", oldValue, localValue);
  }

  if (localValue != oldValue) {
    printf("[SetpointManager] Setpoint changed: %u -> %u ppm (MAIN menu)\n", oldValue, localValue);
    notifyCallbacks(localValue);
    lastSaveTick = xTaskGetTickCount(); // Reset save timer on change
  }
}