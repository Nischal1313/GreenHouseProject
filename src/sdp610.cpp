#include "mutexGuard.h"
#include "sdp610.h"
#include <cstdio>
#include <cmath>
#include "pico/stdlib.h"

static constexpr uint8_t CMD_TRIGGER_MEAS = 0xF1;
static constexpr uint8_t CMD_SOFT_RESET   = 0xFE;
static constexpr float SCALE_FACTOR       = 240.0f; // counts per Pa

SDP610::SDP610(i2c_inst_t *i2c_instance, uint sda_pin_, uint scl_pin_,
               SemaphoreHandle_t mutex, uint8_t i2c_address)
    : i2c(i2c_instance),
      sda_pin(sda_pin_),
      scl_pin(scl_pin_),
      busMutex(mutex),
      address(i2c_address),
      initialized(false) { }

bool SDP610::init() {
    gpio_set_function(sda_pin, GPIO_FUNC_I2C);
    gpio_set_function(scl_pin, GPIO_FUNC_I2C);
    gpio_pull_up(sda_pin);
    gpio_pull_up(scl_pin);

    sleep_ms(10);

    // Send soft reset (0xFE)
    uint8_t cmd = CMD_SOFT_RESET;
    int written = i2c_write_blocking(i2c, address, &cmd, 1, false);
    if (written != 1) {
        printf("SDP610: Soft reset failed\n");
        // Not fatal — some modules don’t need it
    } else {
        printf("SDP610: Soft reset OK\n");
    }

    sleep_ms(50); // sensor warm-up

    initialized = true;
    return true;
}

int16_t SDP610::readRawPressure() const {
    if (!initialized) return INT16_MIN;

    uint8_t cmd = CMD_TRIGGER_MEAS;
    if (i2c_write_blocking(i2c, address, &cmd, 1, false) != 1) {
        printf("SDP610: Failed to send trigger command\n");
        return INT16_MIN;
    }

    sleep_ms(5); // give sensor time

    uint8_t buf[2];
    int read = i2c_read_blocking(i2c, address, buf, 2, false);
    if (read != 2) {
        printf("SDP610: Failed to read measurement\n");
        return INT16_MIN;
    }

    int16_t raw = (int16_t)((buf[0] << 8) | buf[1]);
    return raw;
}

float SDP610::readPressurePa(float altitude_m) const {
    int16_t raw = readRawPressure();
    if (raw == INT16_MIN) return NAN;

    float pressure = static_cast<float>(raw) / SCALE_FACTOR;
    return pressure * altitudeCorrection(altitude_m);
}

float SDP610::altitudeCorrection(float altitude_m) {
    if (altitude_m == 0.0f) return 1.0f;
    return powf(1.0f - (0.0065f * altitude_m / 288.15f), 5.255f);
}
