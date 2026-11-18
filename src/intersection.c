#include "intersection.h"
#include <limits.h>

#include <stdlib.h>

bool intersection_init(SimulationState *state) {
    if (state == NULL) {    // check if state exists
        return false;
    }

    for (int d = 0; d < DIR_COUNT; d++) {
        int count = state->approaches[d].laneCount;    // variable for number of lanes for each approach

        if (count < 1 || count > MAX_LANES_PER_APPROACH) {      // validate if is in the limit
            intersection_free(state);
            return false;
        }

        Approach* approach = &state->approaches[d];
        approach->direction = (Direction)d;
        approach->laneCount = count;
        approach->lanes = calloc((size_t)count, sizeof(LaneQueue));     // memory allocation

        if (approach->lanes == NULL) {      // check if calloc succeeded
            intersection_free(state);
            return false;
        }

        for (int l = 0; l < count; l++) {   // initialize all queues
            queue_init(&approach->lanes[l]);
        }
    }
    return true;
}


bool intersection_reconfigure(SimulationState *state) {
    if (state == NULL) {    // check if state exists
        return false;
    }

    for (int d = 0; d < DIR_COUNT; d++) {
        int count = state->approaches[d].laneCount;    // variable for number of lanes for each approach
        if (count < 1 || count > MAX_LANES_PER_APPROACH) {      // validate if is in the limit
            return false;
        }
    }

    intersection_free(state);
    return intersection_init(state);
}


void intersection_free(SimulationState *state) {
    if (state == NULL) {
        return;
    }

    for (int d = 0; d < DIR_COUNT; d++) {

        Approach* approach = &state->approaches[d];

        if (approach->lanes != NULL) {      // check if calloc succeeded
            for (int l = 0; l < approach->laneCount; l++) {
                queue_free(&approach->lanes[l]);
            }
            free(approach->lanes);
            approach->lanes = NULL;
        }

        approach->laneCount = 0;
    }

    state->activeCrossingCount = 0;
}


int intersection_select_best_lane(const Approach* approach, TurnIntent turn) {
    if (approach == NULL || approach->lanes == NULL || approach->laneCount <= 0) {  // check if approach/lanes exists
        return -1;
    }

    if (turn < 0 || turn > TURN_COUNT) {    // validate turn index
        return -1;
    }

    int bestLane = -1;
    int minCount = INT_MAX;

    for (int l = 0; l < approach->laneCount; l++) {
        if (!approach->lanes[l].allowedTurns[turn]) {   // check if can vehicle can turn on certain lane, if not it is not taken into consideration
            continue;
        }

        if (approach->lanes[l].count < minCount) {
            minCount = approach->lanes[l].count;
            bestLane = l;
        }
    }

    return bestLane;
}