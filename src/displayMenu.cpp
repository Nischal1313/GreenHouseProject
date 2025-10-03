#include "displayMenu.h"

#include <algorithm>

#include "pico/time.h"
#include <cstdio>
#include <cstring>

// Configure priorities (adjust if you prefer)
static constexpr UBaseType_t ENCODER_TASK_PRIO = tskIDLE_PRIORITY + 4;
static constexpr UBaseType_t DISPLAY_TASK_PRIO = tskIDLE_PRIORITY + 2;

// Timing constants
static constexpr TickType_t REVERT_DELAY = pdMS_TO_TICKS(3000);  // 3s
static constexpr TickType_t LOCK_TIME    = pdMS_TO_TICKS(40000); // 40s

// Forward declare to allow ISR static access
static HANDLEC02INPUT* s_instance_for_isr = nullptr;

HANDLEC02INPUT::HANDLEC02INPUT()
    : desiredCO2Value(-1),
      lockedValue(-1),
      locked(false),
      lockTick(0),
      lastChangeTick(0),
      gpioSem(nullptr),
      displayQueue(nullptr),
      encoderTaskHandle(nullptr),
      displayWorkerHandle(nullptr)
{
    // Create binary semaphore (used for encoder A edge notifications)
    gpioSem = xSemaphoreCreateBinary();

    // Create queue for display messages
    displayQueue = xQueueCreate(10, sizeof(DisplayMsg));

    // Initialize GPIO pins (pull-ups)
    gpio_init(ENCODER_PIN_A);
    gpio_set_dir(ENCODER_PIN_A, GPIO_IN);
    gpio_pull_up(ENCODER_PIN_A);

    gpio_init(ENCODER_PIN_B);
    gpio_set_dir(ENCODER_PIN_B, GPIO_IN);
    gpio_pull_up(ENCODER_PIN_B);

    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);

    // attach global pointer for ISR to find this instance
    s_instance_for_isr = this;

    // Configure ISR for both rising and falling edges on ENCODER_PIN_A
    // The Pico SDK uses one global callback; supply the function below
    gpio_set_irq_enabled_with_callback(ENCODER_PIN_A, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &HANDLEC02INPUT::gpio_isr);

    // Prepare initial display message (show '?' meaning unset)
    if (displayQueue) {
        DisplayMsg m{};
        snprintf(m.text, sizeof(m.text), "CO2 set -1?");
        m.y = 50;
        xQueueSend(displayQueue, &m, 0);
    }
}

HANDLEC02INPUT::~HANDLEC02INPUT() {
    // Clean up (optional for embedded long-running)
    if (gpioSem) vSemaphoreDelete(gpioSem);
    if (displayQueue) vQueueDelete(displayQueue);
    s_instance_for_isr = nullptr;
}

int HANDLEC02INPUT::getDesiredValue() const {
    return desiredCO2Value; // -1 if unset
}

// ISR - minimal work: give semaphore from ISR
void HANDLEC02INPUT::gpio_isr(uint gpio, uint32_t events) {
    (void) events;
    if (!s_instance_for_isr) return;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(s_instance_for_isr->gpioSem, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// Start tasks; pass the shared oled pointer so worker can draw directly.
// We pass a small params struct via pv to displayWorkerFn.
void HANDLEC02INPUT::startTasks(const std::shared_ptr<ssd1306os>& oled) {
    // create a small struct to pass both oled and this pointer to display worker
    struct Params {
        std::shared_ptr<ssd1306os> oled;
        HANDLEC02INPUT* self;
    };

    auto* p = new Params{oled, this};

    // encoder task: waits on gpioSem then updates value accordingly
    xTaskCreate(encoderTaskFn, "EncoderTask", 512, this, ENCODER_TASK_PRIO, &encoderTaskHandle);

    // display worker draws messages coming from the internal queue
    xTaskCreate(displayWorkerFn, "DisplayWorker", 1024, p, DISPLAY_TASK_PRIO, &displayWorkerHandle);
}

// encoder task implementation
void HANDLEC02INPUT::encoderTaskFn(void* pv) {
    auto* self = static_cast<HANDLEC02INPUT*>(pv);
    // read initial A state for edge detection fallback
    int lastA = gpio_get(ENCODER_PIN_A);

    for (;;) {
        // wait until ISR gives semaphore (an edge)
        if (xSemaphoreTake(self->gpioSem, pdMS_TO_TICKS(1000)) == pdTRUE) {
            // on each edge, read both pins to determine direction
            int a = gpio_get(ENCODER_PIN_A);
            int b = gpio_get(ENCODER_PIN_B);

            // If currently locked, ignore rotations
            if (!self->locked) {
                // first movement after unset: initialize mid value
                if (self->desiredCO2Value < 0) {
                    self->desiredCO2Value = 400; // choose sensible default
                }
//TODO:VALUE TO THE EEPROM AND READ FROM THERE WHEN THE PROGRAM STARTS.
                // rotate detection: increase/decrease by 10 per event
                // rotate detection: increase/decrease by 10 per event
                if (a != lastA) {
                    // clockwise = increase
                    if (a == b) self->desiredCO2Value += 10;
                    else self->desiredCO2Value -= 10;

                    // Since clamp does not take in volatiles.
                    int tmp = self->desiredCO2Value;
                    tmp = std::clamp(tmp, 200, 1500);
                    self->desiredCO2Value = tmp;

                    // publish update to display queue
                    if (self->displayQueue) {
                        DisplayMsg m{};
                        snprintf(m.text, sizeof(m.text), "CO2 set: %d", self->desiredCO2Value);
                        m.y = 50;
                        xQueueSend(self->displayQueue, &m, 0);
                    }

                    lastA = a;
                }
                lastA = a;
            }

            // Check button for press -> lock the current value
            // Debounce simple: require button low for a short time
            if (gpio_get(BUTTON_PIN) == 0) {
                // simple debounce
                vTaskDelay(pdMS_TO_TICKS(20));
                if (gpio_get(BUTTON_PIN) == 0) {
                    // lock
                    self->locked = true;
                    self->lockTick = xTaskGetTickCount();
                    self->lockedValue = self->desiredCO2Value;

                    if (self->displayQueue) {
                        DisplayMsg m{};
                        snprintf(m.text, sizeof(m.text), "CO2 locked: %d ppm", self->lockedValue);
                        m.y = 50;
                        xQueueSend(self->displayQueue, &m, 0);
                    }
                    // wait until button release
                    while (gpio_get(BUTTON_PIN) == 0) vTaskDelay(pdMS_TO_TICKS(10));
                }
            }
        } else {
            // timeout: every 1000 ms we still check for lock expiry and revert logic

            // unlock after LOCK_TIME
            if (self->locked) {
                if ((xTaskGetTickCount() - self->lockTick) >= LOCK_TIME) {
                    self->locked = false;
                    // send message that lock expired
                    if (self->displayQueue) {
                        DisplayMsg m{};
                        snprintf(m.text, sizeof(m.text), "CO2 unlock: %d ppm", self->desiredCO2Value >= 0 ? self->desiredCO2Value : -1);
                        m.y = 50;
                        xQueueSend(self->displayQueue, &m, 0);
                    }
                }
            } else {
                // revert behavior: if user rotated but didn't press for REVERT_DELAY, revert to lockedValue
                if (self->lastChangeTick != 0 && (xTaskGetTickCount() - self->lastChangeTick) >= REVERT_DELAY) {
                    // If there is a lockedValue defined, revert
                    if (self->lockedValue >= 0 && self->desiredCO2Value != self->lockedValue) {
                        self->desiredCO2Value = self->lockedValue;
                        if (self->displayQueue) {
                            DisplayMsg m{};
                            snprintf(m.text, sizeof(m.text), "CO2 revert: %d ppm", self->desiredCO2Value);
                            m.y = 50;
                            xQueueSend(self->displayQueue, &m, 0);
                        }
                    }
                    // clear lastChangeTick so we don't repeatedly send reverts
                    self->lastChangeTick = 0;
                }
            }
        }
    }
}

// display worker: consumes displayQueue and draws to OLED
void HANDLEC02INPUT::displayWorkerFn(void* pv) {
    // pv is a small heap-alloc Params struct allocated in startTasks
    struct Params {
        std::shared_ptr<ssd1306os> oled;
        HANDLEC02INPUT* self;
    };

    auto* p = static_cast<Params*>(pv);
    auto oled = p->oled;
    auto* self = p->self;

    // local buffer for dequeue
    DisplayMsg msg;

    for (;;) {
        // wait indefinitely for messages
        if (xQueueReceive(self->displayQueue, &msg, portMAX_DELAY) == pdTRUE) {
            // draw the message; we assume other tasks draw other rows
            // lock I2C if your system needs it — here we assume oled->text/show are thread-safe or protected externally
            oled->fill(0);
            oled->text(msg.text, 0, msg.y);
            oled->show();
        }
    }

    // never reached, but if we ever exit
    delete p;
}
