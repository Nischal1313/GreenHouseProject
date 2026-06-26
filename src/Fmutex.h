/*
 * Fmutex.h
 *
 *  Created on: 15.8.2017
 *      Author: krl
 */

#pragma once

#include "FreeRTOS.h"
#include "semphr.h"

class Fmutex
{
public:
    Fmutex();
    ~Fmutex();
    void lock();
    void unlock();

private:
    SemaphoreHandle_t mutexM;
};
