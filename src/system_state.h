#pragma once

#include "state_machine.h"

/// @brief Master state table defined in system_state.cpp
extern StateDefinition const stateTableM[];

/// @brief Number of states in the table
static constexpr uint8_t STATE_TABLE_COUNT = 13;
