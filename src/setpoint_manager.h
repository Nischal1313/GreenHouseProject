#pragma once
#include <functional>
#include <vector>
#include "mutexGuard.h"

class SetpointManager {
public:
  enum class MenuState {
    MAIN,
    WIFI,
  };

  explicit SetpointManager(SemaphoreHandle_t mutex);

  void updateLocal(int value, MenuState activeMenu);

  void updateCloud(int value);

  [[nodiscard]] int getEffectiveTarget() const;

  void subscribeToSetpointChanges(const std::function<void(int)> &cb);

private:
  SemaphoreHandle_t mutex;
  uint cloudValue;
  uint localValue;
  uint32_t cloudTimestampMs;
  uint32_t localTimestampMs;
  std::vector<std::function<void(int)> > callbacks_;

  void notifyCallbacks(int newValue) const;
};
