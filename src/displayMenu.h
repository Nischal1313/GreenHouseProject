#ifndef DISPLAY_SYSTEM_H
#define DISPLAY_SYSTEM_H

#include <queue>
#include "ssd1306os.h"
#include "hardware/gpio.h"
#include "pico/time.h"
#include "FreeRTOS.h"
#include "task.h"
#include <memory>
#include <functional>
#include "semphr.h" // Added for SemaphoreHandle_t
#include "pico/sem.h"

enum class DisplayItemType {
    CO2,
    HUMIDITY,
    TEMPERATURE,
    PRESSURE,
    FAN_STATUS
};

struct DisplayEvent {
    DisplayItemType type;
    float value;
    bool boolValue; // For fan status
    uint32_t timestamp;
};

class DisplaySystem {
public:
    // Added semaphore to constructor signature
    explicit DisplaySystem(std::shared_ptr<ssd1306os> screen, uint8_t buttonPin = 8, uint8_t encoderPinA = 10, uint8_t encoderPinB = 11);

    // Methods to update display items (similar to debug->print)
    void item(DisplayItemType type, float value) const;

    void item(DisplayItemType type, bool value) const; // For boolean values like fan status

    // Set callbacks for control actions
    void setFanSpeedCallback(std::function<void(float)> callback);

    void setValveOpenCallback(std::function<void()> callback);

    // Internal method for display task
    [[nodiscard]] DisplayEvent getEvent() const;

    void processInput(); // Handle button and encoder input
    void updateDisplay(); // Update the display

private:
    QueueHandle_t m_queue;
    std::shared_ptr<ssd1306os> screen;

    // Input handling
    uint8_t buttonPin;
    uint8_t encoderPinA;
    uint8_t encoderPinB;

    // Menu state
    enum MenuState {
        MENU_SENSORS_1, // CO2 and Humidity
        MENU_SENSORS_2, // Temperature and Pressure
        MENU_VALVE, // Valve control
        MENU_FAN_SPEED // Fan speed control
    };

    MenuState currentMenu;
    bool lastButtonState;
    int lastEncoderA;
    float fanSpeed;

    // Latest sensor values
    float co2Value;
    float humidityValue;
    float temperatureValue;
    float pressureValue;
    bool fanRunningValue;

    // Callbacks
    std::function<void(float)> fanSpeedCallback;
    std::function<void()> valveOpenCallback;

    absolute_time_t lastButtonPress;
    static constexpr uint32_t DEBOUNCE_TIME_US = 200000; // 200ms
};

class DisplayTask {
public:
    explicit DisplayTask(std::shared_ptr<DisplaySystem> displaySystem);

    [[noreturn]] void run() const;

private:
    std::shared_ptr<DisplaySystem> m_displaySystem;
    QueueHandle_t m_queue{};
    std::shared_ptr<ssd1306os> screen;
};

#endif // DISPLAY_SYSTEM_H
