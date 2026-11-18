#ifndef IO_H
#define IO_H

#include <stdbool.h>
#include <stddef.h>
#include "types.h"

#define MAX_BUFFER_SIZE 16              // maximal lenght of one line in scenario
#define INITIAL_SCENARIO_CAPACITY 32    // initial scenario capacity (can be extended if needed)

bool io_load_scenario(const char* path, ScenarioQueue* scenario, char* errorBuffer, size_t errorSize);
void io_free_scenario(ScenarioQueue *scenario);

bool io_save_state(const char* path, const SimulationState* state, char* errorBuffer, size_t errorSize);
bool io_load_state(const char* path, SimulationState* state, char* errorBuffer, size_t errorSize);

#endif