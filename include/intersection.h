#ifndef INTERSECTION_H
#define INTERSECTION_H

#include "types.h"
#include "queue.h"
#include <stdbool.h>
#include <stdlib.h>

bool intersection_init(SimulationState *state);
bool intersection_reconfigure(SimulationState *state);
void intersection_free(SimulationState *state);
int intersection_select_best_lane(const Approach* approach, TurnIntent turn);

#endif