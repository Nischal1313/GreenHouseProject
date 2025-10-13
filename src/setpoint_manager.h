#pragma once
#include <functional>
#include <memory>
#include <vector>
#include "mutexGuard.h"
#include "eeprom/eeprom.h"
#include "rotary_encoder.h"

class SetpointManager {
public:
  enum class MenuState {
    MAIN,
    WIFI,
  };

  explicit SetpointManager(SemaphoreHandle_t mutex, const std::shared_ptr<Eeprom> &,
                           const std::shared_ptr<RotaryEncoder> &);

  void updateLocal(int value, MenuState activeMenu);

  void updateCloud(int value);

  [[nodiscard]] int getEffectiveTarget() const;

  void subscribeToSetpointChanges(const std::function<void(int)> &cb);

  void updateFromEncoder(MenuState currentMenu);  // ADDED: Pass current menu state

private:
  SemaphoreHandle_t mutex;
  std::shared_ptr<Eeprom> eeprom;  // Changed to shared_ptr to avoid copy
  std::shared_ptr<RotaryEncoder> encoder;  // Changed to shared_ptr - CRITICAL FIX
  uint cloudValue;
  uint localValue;
  uint eepromValue;
  uint32_t cloudTimestampMs;
  uint32_t localTimestampMs;
  std::vector<std::function<void(int)>> callbacks_;
  static constexpr uint16_t co2Address = 0x00;
  static constexpr uint16_t MIN_CO2 = 200;
  static constexpr uint16_t MAX_CO2 = 1500;
  static constexpr uint32_t EEPROM_SAVE_INTERVAL_MS = 10000; // 10 seconds
  static constexpr uint32_t cloudExpirationSeconds = 10000;
  TickType_t lastSaveTick;
  const TickType_t saveIntervalTicks = pdMS_TO_TICKS(EEPROM_SAVE_INTERVAL_MS);

  void notifyCallbacks(int newValue) const;

  void writeToEEPROM();
};