#include "rotary_encoder.h"

/*
 * Gray-code rotary encoder logic with button press/hold and debouncing.
 *
 * A/B signals form this Gray code pattern:
 *   CW:  00 -> 01 -> 11 -> 10 -> 00
 *   CCW: 00 -> 10 -> 11 -> 01 -> 00
 *
 * We use both last and current A/B states (2 bits each) to build a
 * 4-bit "transition code". Valid CW and CCW transitions are matched
 * against known patterns. This method filters bounce and invalid states.
 */

RotaryEncoder::RotaryEncoder(uint pinAP, uint pinBP,
                             uint pinButtonP,
                             uint32_t debounceTimeP,
                             uint32_t holdTimeP)
  : pinAM{pinAP}, pinBM{pinBP}, pinButtonM{pinButtonP},
    lastEncodedM{0}, cwEventM{false}, ccwEventM{false},
    lastButtonReadingM{false}, buttonStateM{false},
    pressedEventM{false}, heldEventM{false},
    debounceTimeM{debounceTimeP}, holdTimeM{holdTimeP}
{
    gpio_init(pinAP);
    gpio_set_dir(pinAP, GPIO_IN);
    gpio_pull_up(pinAP);
    gpio_init(pinBP);
    gpio_set_dir(pinBP, GPIO_IN);
    gpio_pull_up(pinBP);
    gpio_init(pinButtonP);
    gpio_set_dir(pinButtonP, GPIO_IN);
    gpio_pull_up(pinButtonP);

    int const MSB = !gpio_get(pinAP);
    int const LSB = !gpio_get(pinBP);
    lastEncodedM = MSB << 1 | LSB;

    lastDebounceTimeM = get_absolute_time();
    pressStartTimeM = get_absolute_time();
}

void RotaryEncoder::update()
{
    int const MSB = !gpio_get(pinAM);
    int const LSB = !gpio_get(pinBM);
    int const encoded = MSB << 1 | LSB;

    // These 8 transitions are valid
    switch (lastEncodedM << 2 | encoded) {
        // CW transitions
        case 0b1101:
        case 0b0100:
        case 0b0010:
        case 0b1011:
            ccwEventM = false;
            cwEventM = true;
            break;

        // CCW transitions
        case 0b1110:
        case 0b0111:
        case 0b0001:
        case 0b1000:
            cwEventM = false;
            ccwEventM = true;
            break;

        default:
            // Ignore invalid
            break;
    }
    lastEncodedM = encoded;

    // ---- BUTTON HANDLING ----
    bool const reading = !gpio_get(pinButtonM); // active low
    absolute_time_t const now = get_absolute_time();

    // Debounce: only consider stable changes after debounceTime ms
    if (reading != lastButtonReadingM) {
        lastDebounceTimeM = now;
    }

    if (absolute_time_diff_us(lastDebounceTimeM, now) > debounceTimeM *
        1000) {
        if (reading != buttonStateM) {
            buttonStateM = reading;
            if (buttonStateM) {
                // pressed
                pressedEventM = true;
                pressStartTimeM = now;
                heldEventM = false;
            } else {
                heldEventM = false;
            }
        }
    }

    if (buttonStateM && !heldEventM &&
        absolute_time_diff_us(pressStartTimeM, now) > holdTimeM * 1000) {
        heldEventM = true;
    }
    lastButtonReadingM = reading;
}

bool RotaryEncoder::rotatedCW()
{
    bool result{false};
    if (cwEventM) {
        cwEventM = false;
        result = true;
    }
    return result;
}

bool RotaryEncoder::rotatedCCW()
{
    bool result{false};
    if (ccwEventM) {
        ccwEventM = false;
        result = true;
    }
    return result;
}

bool RotaryEncoder::buttonPressed()
{
    bool result{false};
    if (pressedEventM) {
        pressedEventM = false;
        result = true;
    }
    return result;
}

bool RotaryEncoder::buttonHeld()
{
    bool result{false};
    if (heldEventM) {
        heldEventM = false;
        result = true;
    }
    return result;
}

void RotaryEncoder::taskEntry(void *pvParametersP)
{
    auto *pEncoder = static_cast<RotaryEncoder *>(pvParametersP);
    while (true) {
        pEncoder->update();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
