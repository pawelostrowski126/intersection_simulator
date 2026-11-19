#ifndef SIMULATION_H
#define SIMULATION_H

#include "types.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>


bool simulation_init_default(SimulationState* state);
bool simulation_reset(SimulationState* state);
void simulation_step(SimulationState* state);   // runs once every framesPerStep frames; discrete traffic logic
void simulation_frame(SimulationState* state);  // visual frame and input clock
void simulation_adjust_speed(SimulationState* state, int value);    // negative value slows down the simulation, positive value speeds it up
void simulation_set_banner(SimulationState* state, const char* message, int durationFrames);
bool simulation_finished(const SimulationState* state);
double simulation_get_average_wait(const SimulationState* state);


#endif