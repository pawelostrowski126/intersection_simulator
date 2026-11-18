#ifndef QUEUE_H
#define QUEUE_H

#include "types.h"

void queue_init(LaneQueue* queue);
bool queue_push(LaneQueue* queue, Vehicle vehicle);
bool queue_pop(LaneQueue* queue, Vehicle* vehicleOut);
bool queue_clear(LaneQueue* queue);
Vehicle* queue_peek(LaneQueue* queue);
bool queue_has_emergency(LaneQueue* queue);
void queue_free(LaneQueue* queue);

#endif