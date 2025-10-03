#include "displayMenu.h"
#include <cstdio>
#include <cmath>
#include <utility>

void displayTaskFunc(void *pvParameters);

// FIX 1: UPDATE CONSTRUCTOR SIGNATURE AND INITIALIZATION LIST
DisplaySystem::DisplaySystem(std::shared_ptr<ssd1306os> screen, const uint8_t buttonPin, uint8_t encoderPinA,
                             uint8_t encoderPinB)
    : m_queue(xQueueCreate(10, sizeof(DisplayEvent))), screen(std::move(screen)),
      buttonPin(buttonPin), encoderPinA(encoderPinA), encoderPinB(encoderPinB),
      currentMenu(MENU_SENSORS_1), lastButtonState(true), lastEncoderA(0), fanSpeed(0.0f),
      co2Value(0), humidityValue(0), temperatureValue(0), pressureValue(0), fanRunningValue(false),
      lastButtonPress(get_absolute_time()) {

    // Initialize button pin (active low with pullup)
    gpio_init(buttonPin);
    gpio_set_dir(buttonPin, GPIO_IN);
    gpio_pull_up(buttonPin);

    // Initialize encoder pins
    gpio_init(encoderPinA);
    gpio_set_dir(encoderPinA, GPIO_IN);
    gpio_pull_up(encoderPinA);

    gpio_init(encoderPinB);
    gpio_set_dir(encoderPinB, GPIO_IN);
    gpio_pull_up(encoderPinB);

    // Read initial encoder state
    lastEncoderA = gpio_get(encoderPinA);
}

void DisplaySystem::item(DisplayItemType type, float value) const {
    DisplayEvent e{};
    e.type = type;
    e.value = value;
    e.boolValue = false;
    e.timestamp = xTaskGetTickCount();

    // Non-blocking send
    xQueueSend(m_queue, &e, 0);
}

void DisplaySystem::item(DisplayItemType type, bool value) const {
    DisplayEvent e{};
    e.type = type;
    e.value = 0.0f;
    e.boolValue = value;
    e.timestamp = xTaskGetTickCount();

    // Non-blocking send
    xQueueSend(m_queue, &e, 0);
}

void DisplaySystem::setFanSpeedCallback(std::function<void(float)> callback) {
    fanSpeedCallback = callback;
}

void DisplaySystem::setValveOpenCallback(std::function<void()> callback) {
    valveOpenCallback = callback;
}

DisplayEvent DisplaySystem::getEvent() const {
    DisplayEvent e{};
    // Non-blocking receive to allow processing input even when no display updates
    if (xQueueReceive(m_queue, &e, 0) == pdTRUE) {
        return e;
    }
    // Return empty event if no data
    e.type = static_cast<DisplayItemType>(-1); // Invalid type indicates no data
    return e;
}

void DisplaySystem::processInput() {
    // Handle button press
    bool currentButtonState = gpio_get(buttonPin);
    absolute_time_t now = get_absolute_time();

    // Detect button press (active low)
    if (lastButtonState && !currentButtonState) {
        // Button pressed, check debounce
        if (absolute_time_diff_us(lastButtonPress, now) > DEBOUNCE_TIME_US) {
            // Navigate to next menu
            switch (currentMenu) {
                case MENU_SENSORS_1:
                    currentMenu = MENU_SENSORS_2;
                    break;
                case MENU_SENSORS_2:
                    currentMenu = MENU_VALVE;
                    break;
                case MENU_VALVE:
                    currentMenu = MENU_FAN_SPEED;
                    break;
                case MENU_FAN_SPEED:
                    currentMenu = MENU_SENSORS_1;
                    break;
            }
            lastButtonPress = now;
        }
    }
    lastButtonState = currentButtonState;

    // Handle encoder
    int currentEncoderA = gpio_get(encoderPinA);
    int currentEncoderB = gpio_get(encoderPinB);

    // Check for encoder rotation
    if (currentEncoderA != lastEncoderA) {
        int direction = (currentEncoderA == currentEncoderB) ? 1 : -1;

        switch (currentMenu) {
            case MENU_VALVE:
                // Encoder rotation opens valve
                if (valveOpenCallback) {
                    valveOpenCallback();
                }
                break;

            case MENU_FAN_SPEED:
                // Encoder adjusts fan speed
                fanSpeed += direction * 5.0f; // 5% increments
                if (fanSpeed < 0.0f) fanSpeed = 0.0f;
                if (fanSpeed > 100.0f) fanSpeed = 100.0f;

                if (fanSpeedCallback) {
                    fanSpeedCallback(fanSpeed);
                }
                break;

            default:
                // No encoder action for sensor display menus
                break;
        }

        lastEncoderA = currentEncoderA;
    }
}

void DisplaySystem::updateDisplay() {
    // Process any pending display events
    DisplayEvent e = getEvent(); // Linker complained this was missing.
    if (static_cast<int>(e.type) != -1) {
        // Valid event
        switch (e.type) {
            case DisplayItemType::CO2:
                co2Value = e.value;
                break;
            case DisplayItemType::HUMIDITY:
                humidityValue = e.value;
                break;
            case DisplayItemType::TEMPERATURE:
                temperatureValue = e.value;
                break;
            case DisplayItemType::PRESSURE:
                pressureValue = e.value;
                break;
            case DisplayItemType::FAN_STATUS:
                fanRunningValue = e.boolValue;
                break;
        }
    }

    // Update screen
    screen->fill(0);

    char line1[32];
    char line2[32];
    char line3[32];
    char line4[32];

    switch (currentMenu) {
        case MENU_SENSORS_1:
            snprintf(line1, sizeof(line1), "SENSORS 1/4");
            snprintf(line2, sizeof(line2), "CO2: %.1f ppm", co2Value);
            snprintf(line3, sizeof(line3), "Humidity: %.1f%%", humidityValue);
            snprintf(line4, sizeof(line4), "Press btn: Next");
            break;

        case MENU_SENSORS_2:
            snprintf(line1, sizeof(line1), "SENSORS 2/4");
            snprintf(line2, sizeof(line2), "Temp: %.1fC", temperatureValue);
            snprintf(line3, sizeof(line3), "Press: %.1f Pa", pressureValue);
            snprintf(line4, sizeof(line4), "Press btn: Next");
            break;

        case MENU_VALVE:
            snprintf(line1, sizeof(line1), "VALVE CTRL 3/4");
            snprintf(line2, sizeof(line2), "Rotate encoder");
            snprintf(line3, sizeof(line3), "to open valve");
            snprintf(line4, sizeof(line4), "Press btn: Next");
            break;

        case MENU_FAN_SPEED:
            snprintf(line1, sizeof(line1), "FAN SPEED 4/4");
            snprintf(line2, sizeof(line2), "Speed: %.0f%%", fanSpeed);
            snprintf(line3, sizeof(line3), "Status: %s", fanRunningValue ? "ON" : "OFF");
            snprintf(line4, sizeof(line4), "Rotate: Adjust");
            break;
    }

    screen->text(line1, 0, 0);
    screen->text(line2, 0, 12);
    screen->text(line3, 0, 24);
    screen->text(line4, 0, 50);

    screen->show();}

DisplayTask::DisplayTask(std::shared_ptr<DisplaySystem> displaySystem)
    : m_displaySystem(std::move(displaySystem)) {
    constexpr int TASK_LOW_PRIORITY = 1 + tskIDLE_PRIORITY;
    xTaskCreate(displayTaskFunc, "DisplayTask", 2048, this, TASK_LOW_PRIORITY, nullptr);
}

[[noreturn]] void DisplayTask::run() const {
    while (true) {
        m_displaySystem->processInput(); // Linker complained this was missing.
        m_displaySystem->updateDisplay();
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void displayTaskFunc(void *pvParameters) {
    auto task = static_cast<DisplayTask *>(pvParameters);
    task->run();
}
