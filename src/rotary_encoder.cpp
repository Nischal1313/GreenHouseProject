#include "rotary_encoder.h"
#include "task.h"

// Initialize static instance pointer
RotaryEncoder* RotaryEncoder::instance = nullptr;

/**
 * @brief Construct a new Rotary Encoder object.
 * @param pinA GPIO pin connected to rotary encoder A phase
 * @param pinB GPIO pin connected to rotary encoder B phase
 * @param pinSW GPIO pin connected to rotary encoder switch/button
 */
RotaryEncoder::RotaryEncoder(uint8_t pinA, uint8_t pinB, uint8_t pinSW)
    : pinA(pinA), pinB(pinB), pinSW(pinSW),
      debounceTimeMs(250), lastButtonPress(0), eventQueue(nullptr) {

    // Set the static instance for ISR access
    instance = this;
}

/**
 * @brief Initialize the rotary encoder hardware.
 *
 * Configures GPIO pins with appropriate directions and pull resistors,
 * and sets up interrupts for rotation detection and button press detection.
 */
void RotaryEncoder::init() {
    // Initialize GPIO pins
    gpio_init(pinA);
    gpio_init(pinB);
    gpio_init(pinSW);

    // Set directions
    gpio_set_dir(pinA, GPIO_IN);
    gpio_set_dir(pinB, GPIO_IN);
    gpio_set_dir(pinSW, GPIO_IN);

    // Enable pull-up for switch pin (active low)
    gpio_pull_up(pinSW);

    // Set up interrupts
    // Detect rising edge on pinA for rotation detection
    gpio_set_irq_enabled_with_callback(pinA, GPIO_IRQ_EDGE_RISE, true, &RotaryEncoder::gpioCallback);
    // Detect falling edge on switch pin for button press detection
    gpio_set_irq_enabled(pinSW, GPIO_IRQ_EDGE_FALL, true);
}

/**
 * @brief Create a FreeRTOS queue for event handling.
 * @param queueLength Maximum number of events the queue can hold
 * @return true if queue creation succeeded, false otherwise
 */
bool RotaryEncoder::createEventQueue(UBaseType_t queueLength) {
    eventQueue = xQueueCreate(queueLength, sizeof(RotaryEvent));
    if (eventQueue == nullptr) {
        return false;
    }
    vQueueAddToRegistry(eventQueue, "RotaryEncoderQueue");
    return true;
}

/**
 * @brief Set the debounce time for button presses.
 * @param debounceMs Debounce time in milliseconds
 */
void RotaryEncoder::setDebounceTime(uint32_t debounceMs) {
    debounceTimeMs = debounceMs;
}

/**
 * @brief Static GPIO interrupt callback function.
 * @param gpio GPIO pin that triggered the interrupt
 * @param events Type of interrupt event
 *
 * This function is called from interrupt context when a GPIO event occurs.
 * It detects rotary encoder rotation and button presses, and adds events
 * to the queue for processing in task context.
 */
void RotaryEncoder::gpioCallback(uint gpio, uint32_t events) {
    // Safety check - ensure instance and queue exist
    if (instance == nullptr || instance->eventQueue == nullptr) {
        return;
    }

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    RotaryEvent event;
    event.time = xTaskGetTickCount();

    if (gpio == instance->pinSW && (events & GPIO_IRQ_EDGE_FALL)) {
        // Button press event with debouncing
        if ((event.time - instance->lastButtonPress) > pdMS_TO_TICKS(instance->debounceTimeMs)) {
            event.type = RotaryEventType::ButtonPress;
            xQueueSendFromISR(instance->eventQueue, &event, &xHigherPriorityTaskWoken);
            instance->lastButtonPress = event.time;
        }
    }
    else if (gpio == instance->pinA && (events & GPIO_IRQ_EDGE_RISE)) {
        // Rotation detection - determine direction based on state of pinB
        if (gpio_get(instance->pinB) == 0) {
            event.type = RotaryEventType::Clockwise;
        } else {
            event.type = RotaryEventType::CounterClockwise;
        }
        xQueueSendFromISR(instance->eventQueue, &event, &xHigherPriorityTaskWoken);
    }

    // Yield if a higher priority task was woken
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}