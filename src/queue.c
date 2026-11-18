#include "queue.h"

#include <stdbool.h>
#include <stdlib.h>


void queue_init(LaneQueue* queue) {
    queue->front = NULL;
    queue->rear = NULL;
    queue->count = 0;
    queue->maxCount = 0;
    for (int i = 0; i < TURN_COUNT; i++) {
        queue->allowedTurns[i] = true;
    }
}


bool queue_push(LaneQueue* queue, Vehicle vehicle) {
    if (queue == NULL) {        // check if queue exists
        return false;
    }
    
    VehicleNode* newNode = malloc(sizeof(VehicleNode));
    if (!newNode) {             // check if malloc succeeded
        return false;
    }

    newNode->vehicle = vehicle;
    newNode->next = NULL;

    if (queue->front == NULL) {
        queue->front = newNode;
        queue->rear = newNode;
    }
    else {
        queue->rear->next = newNode;
        queue->rear = newNode;
    }

    queue->count++;
    return true;
}


bool queue_pop(LaneQueue* queue, Vehicle* vehicleOut) {
    if (queue == NULL || queue->front == NULL) {    // check if queue exists and if has any vehicles
        return false;
    }

    VehicleNode* tempNode = queue->front;   // temporary storage for removed car
    if (vehicleOut) {
        *vehicleOut = tempNode->vehicle;
    }

    queue->front = queue->front->next;
    if (queue->front == NULL) {
        queue->rear == NULL;
    }

    free(tempNode);
    queue->count--;
    return true;
}


bool queue_clear(LaneQueue* queue) {
    if (queue == NULL) {
        return false;
    }

    VehicleNode* currentNode =  queue->front;
    while (currentNode == NULL) {
        VehicleNode* next = currentNode->next;
        free(currentNode);
        currentNode = next;
    }

    queue->front = NULL;
    queue->rear = NULL;
    queue->count = 0;


    return true;
}


Vehicle* queue_peek(LaneQueue* queue) {
    if (queue == NULL || queue->front == NULL) {    // check if queue exists and if has any vehicles
        return NULL;
    }
    return &(queue->front->vehicle);
}


bool queue_has_emergency(LaneQueue* queue) {
    if (queue == NULL) {    // check if queue exists
        return false;
    }

    VehicleNode* currentNode = queue->front;        // go through all vehicles in queue and check if it contains any emergency vehicle
    for (; currentNode!=NULL; currentNode=currentNode->next) {
        if (currentNode->vehicle.isEmergency == true) {
            return true;
        }
    }
    return false;
}

void queue_free(LaneQueue* queue) {
    if (queue == NULL) {    // check if queue exists
        return;
    }

    VehicleNode* currentNode = queue->front;    // free all nodes from these queue
    while (currentNode != NULL) {
        VehicleNode* nextNode = currentNode->next;
        free(currentNode);
        currentNode = nextNode;
    }
    
    queue->count = 0;       // return queue to its initial state 
    queue->maxCount = 0;
    for (int i = 0; i < TURN_COUNT; i++) {
        queue->allowedTurns[i] = true;
    }
    queue->front = NULL;
    queue->rear = NULL;
}

