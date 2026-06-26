/*
 * Fmutex.cpp
 *
 *  Created on: 15.8.2017
 *      Author: krl
 */

#include "Fmutex.h"

Fmutex::Fmutex()
{
    mutexM = xSemaphoreCreateMutex();
}

Fmutex::~Fmutex()
{
    vSemaphoreDelete(mutexM);
}

void Fmutex::lock()
{
    xSemaphoreTake(mutexM, portMAX_DELAY);
}

void Fmutex::unlock()
{
    xSemaphoreGive(mutexM);
}
