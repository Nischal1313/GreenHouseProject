#include "setpoint_manager.h"
#include "pico/time.h"

SetpointManager::SetpointManager(const SemaphoreHandle_t mutex)
    : mutex(mutex), cloudValue(0), localValue(0),
      cloudTimestampMs(0), localTimestampMs(0) {}

void SetpointManager::updateLocal(const int value, const MenuState activeMenu) {
  if (activeMenu != MenuState::MAIN) return; // only update when main menu is active

  MutexGuard lock(mutex);
  if (!lock.owns_lock()) return;

  localValue = value;
  localTimestampMs = to_ms_since_boot(get_absolute_time());
  notifyCallbacks(getEffectiveTarget());
}

void SetpointManager::updateCloud(const int value) {
  MutexGuard lock(mutex);
  if (!lock.owns_lock()) return;

  cloudValue = value;
  cloudTimestampMs = to_ms_since_boot(get_absolute_time());
  notifyCallbacks(getEffectiveTarget());
}

int SetpointManager::getEffectiveTarget() const {
  MutexGuard lock(mutex);
  if (!lock.owns_lock()) return localValue;

  uint32_t nowMs = to_ms_since_boot(get_absolute_time());
  if ((nowMs - cloudTimestampMs) < 1000) return cloudValue;
  return localValue;
}

void SetpointManager::subscribeToSetpointChanges(const std::function<void(int)> &cb) {
  MutexGuard lock(mutex);
  if (!lock.owns_lock()) return;
  callbacks_.push_back(cb);
}

void SetpointManager::notifyCallbacks(const int newValue) const {
  for(auto &cb : callbacks_) cb(newValue);
}
