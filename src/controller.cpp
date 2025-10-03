// Controller.cpp (the FreeRTOS task)
#include "encoderHandler.h"
#include "displayHandler.h"

#define ROT_A 10
#define ROT_B 11

static EncoderHandler encoder(ROT_A, ROT_B);
static DisplayHandler display;

static int32_t desiredValue = 300;
static int32_t appliedValue = 300;
static TickType_t lastChangeTime = 0;

[[noreturn]] void encoderTask(void* param) {
    encoder.init();
    display.init();
    display.showValue(desiredValue);

    while (true) {
        int delta = encoder.readDelta();
        if (delta != 0) {
            desiredValue += delta;
            if (desiredValue < 0) desiredValue = 0;
            if (desiredValue > 2000) desiredValue = 2000;
            lastChangeTime = xTaskGetTickCount();
            display.showValue(desiredValue);
        }

        if ((xTaskGetTickCount() - lastChangeTime) > pdMS_TO_TICKS(40000)) {
            appliedValue = desiredValue;
        }

        vTaskDelay(pdMS_TO_TICKS(40)); // 50 Hz poll
    }
}
