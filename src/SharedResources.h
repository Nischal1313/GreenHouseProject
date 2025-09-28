//
// Created by Amaan on 28/09/2025.
//
#pragma once
#include <memory>
#include "semphr.h"

struct SharedResources {
    float co2_ppm;
    float humidity;
    float temperature;
    float fan_speed;
    float co2_setpoint;

    SemaphoreHandle_t mutex;
};
extern std::shared_ptr<SharedResources> g_sharedResources;