#include "io.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>
#include <float.h>
#include "queue.h"

static bool read_quoted_string(FILE* file, char* output, size_t outputSize) {
    int character;
    size_t length = 0;

    do {
        character = fgetc(file);
    } while (character != EOF && isspace((unsigned char)character));

    if (character != '"') {
        return false;
    }

    while ((character = fgetc(file)) != EOF && character != '"') {
        if (character == '\n' || length + 1 >= outputSize) {
            return false;
        }
        output[length++] = (char)character;
    }

    if (character != '"') {
        return false;
    }
    output[length] = '\0';
    return true;
}

// parsing certain values from certain events
static bool parse_direction(const char* event, Direction* outputDirection) {
    if (event == NULL || event[0] == '\0' || event[1] == '\0') {
        return false;
    }
    switch ((char)toupper((unsigned char)event[0])) {
        case 'N':
            *outputDirection = NORTH;
            return true;
        case 'S':
            *outputDirection = SOUTH;
            return true;
        case 'E':
            *outputDirection = EAST;
            return true;
        case 'W':
            *outputDirection = WEST;
            return true;
        default: 
            return false;
    }
}

static bool parse_turn(const char* event, TurnIntent* outputTurn) {
    if (event == NULL || event[0] == '\0' || event[1] == '\0') {
        return false;
    }
    switch ((char)toupper((unsigned char)event[0])) {
        case 'S':
            *outputTurn = STRAIGHT;
            return true;
        case 'L':
            *outputTurn = LEFT;
            return true;
        case 'R':
            *outputTurn = RIGHT;
            return true;
        default: 
            return false;
    }
}

static bool parse_emergency(const char* event) {
    if (event == NULL || event[0] == '\0' || event[1] == '\0') {
        return false;
    }
    return (event[0] == '!');
}


// compare events (for sorting by arrival step)
static int compare_events(const void* a, const void* b) {
    const ScenarioEvent* eventA = (const ScenarioEvent*)a;
    const ScenarioEvent* eventB = (const ScenarioEvent*)b;

    if (eventA->arrivalStep < eventB->arrivalStep) return -1;
    else if (eventA->arrivalStep > eventB->arrivalStep) return -1;
    else return 0;
}



void io_free_scenario(ScenarioQueue *scenario) {
    if (scenario == NULL) {
        return;
    }

    free(scenario->events);
    scenario->capacity = 0;
    scenario->count = 0;
    scenario->events = NULL;
    scenario->nextIndex = 0;
}

static void io_free_loaded_state(SimulationState* state) {
    for (int d = 0; d < DIR_COUNT; d++) {
        Approach* approach = &state->approaches[d];
        if (approach->lanes != NULL) {
            for (int l = 0; l < approach->laneCount; l++) {
                queue_free(&approach->lanes[l]);
            }
            free(approach->lanes);
            approach->lanes = NULL;
        }
        approach->laneCount = 0;
    }

    free(state->scenarioQueue.events);
    state->scenarioQueue.events = NULL;
    state->scenarioQueue.count = 0;
    state->scenarioQueue.capacity = 0;
    state->scenarioQueue.nextIndex = 0;
}

static bool io_state_load_error(FILE* file, SimulationState* state,
                                char* errorBuffer, size_t errorSize,
                                const char* message) {
    if (errorBuffer != NULL && errorSize > 0) {
        snprintf(errorBuffer, errorSize, "ERROR: %s", message);
    }
    io_free_loaded_state(state);
    fclose(file);
    return false;
}


bool io_load_scenario(const char* path, ScenarioQueue* scenario, char* errorBuffer, size_t errorSize) {
    // check for errors
    if (path == NULL || scenario == NULL) {     // check if path or scenario exists
        if (errorBuffer && errorSize > 0) {
            snprintf(errorBuffer, errorSize, "ERROR: NULL pointer passed to loader!");      // display proper error
        }
        return false;
    }

    FILE* file = fopen(path, "r");
    if (file == NULL) {     // check if file was opened
        if (errorBuffer && errorSize > 0) {
            snprintf(errorBuffer, errorSize, "ERROR: Cannot open file '%s'!", path);        // display proper error
        }
        return false;
    }

    // build into temporary scenario so an invalid file doesnt wipe an existing one
    ScenarioQueue temp = {0};
    temp.capacity = INITIAL_SCENARIO_CAPACITY;
    temp.events = malloc(temp.capacity * sizeof(ScenarioEvent));
    if (temp.events == NULL) {      // check if malloc succeeded
        fclose(file);
        if (errorBuffer && errorSize > 0) {
            snprintf(errorBuffer, errorSize, "ERROR: Memory allocation failed while loading scenario!");        // display proper error
        }
        return false;
    }

    char line[MAX_BUFFER_SIZE];
    int lineNumber = 0;
    const char* delimiterCharacters = " \t\r\n";

    while (fgets(line, sizeof(line), file) != NULL) {
        lineNumber++;

        if (strchr(line, '\n') == NULL || !feof(file)) {    // check if there is an error with the string or if its end of the file(eof)
            snprintf(errorBuffer, errorSize, "ERROR: Line %d exceeds maximum lenght!", lineNumber);        // display proper error
            
            io_free_scenario(&temp);
            fclose(file);
            return false;
        }

        // arrival step
        char* eventStep = strtok(line, delimiterCharacters);    // contains arrival step of certain event in string format
        if (eventStep == NULL || eventStep[0] == '#'){      // check if line is empty or is a comment(#)
            continue;
        }

        char* endptr = NULL;
        long step = strtol(eventStep, &endptr, 10);

        if (*endptr != '\0' || step < 0 || step > INT_MAX) {
            snprintf(errorBuffer, errorSize, "ERROR: invalid simulation step %d, expected integer!", lineNumber);        // display proper error
            
            io_free_scenario(&temp);
            fclose(file);
            return false;
        }
    
        // direction
        char* eventDirection = strtok(NULL, delimiterCharacters);
        Direction direction;

        if (!parse_direction(eventDirection, &direction)) {
            snprintf(errorBuffer, errorSize, "ERROR: invalid simulation step %d, expected proper direction!", lineNumber);        // display proper error

            io_free_scenario(&temp);
            fclose(file);
            return false;
        }

        // turn and emergency
        char* eventTurnOrEmergency = strtok(NULL, delimiterCharacters);
        TurnIntent turn;
        bool isEmergency = false;
        
        if (eventTurnOrEmergency != NULL) {
            if (parse_emergency(eventTurnOrEmergency)) {
                isEmergency = true;
            }
            else if (parse_turn(eventTurnOrEmergency, &turn)) {
                char* eventEmergency = strtok(NULL, delimiterCharacters);
                if (eventEmergency != NULL) {
                    if (parse_emergency(eventEmergency)) {
                        isEmergency = true;
                    }
                }
                else {
                    snprintf(errorBuffer, errorSize, "ERROR: invalid simulation step %d, invalid token!", lineNumber);        // display proper error

                    io_free_scenario(&temp);
                    fclose(file);
                    return false;
                }
            }
            else {
                    snprintf(errorBuffer, errorSize, "ERROR: invalid simulation step %d, invalid turn/flag!", lineNumber);        // display proper error

                    io_free_scenario(&temp);
                    fclose(file);
                    return false;
                }
        }
        
        // check if strtok() does not have any garbage left
        char* unexpectedToken = strtok(NULL, delimiterCharacters);
        if (unexpectedToken != NULL) {
                    snprintf(errorBuffer, errorSize, "ERROR: invalid simulation step %d, unexpected token!", lineNumber);        // display proper error

                    io_free_scenario(&temp);
                    fclose(file);
                    return false;
                }

        // grow scenario array if needed
        if (temp.count >= temp.capacity) {
            size_t newCapacity = 2 * temp.capacity;
            ScenarioEvent* resizedEvents = realloc(temp.events, newCapacity * sizeof(ScenarioEvent));

            if (resizedEvents == NULL) {
                snprintf(errorBuffer, errorSize, "ERROR: Memory allocation failed when resizing scenario!", lineNumber);        // display proper error

                io_free_scenario(&temp);
                fclose(file);
                return false;
            }

            temp.events = resizedEvents;
            temp.capacity = newCapacity;
        }

        // assign values
        temp.events[temp.count++] = (ScenarioEvent) {
            .arrivalStep = (int)step,
            .sideEntered = direction,
            .turn = turn,
            .isEmergency = isEmergency
        };
    }

    fclose(file);

    // check if there are any instrucitons in scenario.txt
    if (temp.count == 0) {
        snprintf(errorBuffer, errorSize, "ERROR: Scenario does not contain any instructions!", lineNumber);        // display proper error

        io_free_scenario(&temp);
        return false;
    }

    // sort array chronlogically
    qsort(temp.events, temp.count, sizeof(ScenarioEvent), compare_events);

    // commit to scenario
    io_free_scenario(scenario);
    *scenario = temp;
    return true;
}


bool io_save_state(const char* path, const SimulationState* state, char* errorBuffer, size_t errorSize) {
    // check for errors
    if (path == NULL || state == NULL) {     // check if path or scenario exists
        if (errorBuffer && errorSize > 0) {
            snprintf(errorBuffer, errorSize, "ERROR: NULL pointer passed to saver!");      // display proper error
        }
        return false;
    }

    FILE* file = fopen(path, "w");
    if (file == NULL) {     // check if file was opened
        if (errorBuffer && errorSize > 0) {
            snprintf(errorBuffer, errorSize, "ERROR: Cannot open file '%s'!", path);        // display proper error
        }
        return false;
    }

    // curren step and next vehicle id
    fprintf(file, "STEP %d %d\n", state->currentStep, state->nextVehicleId);

    // lights
    fprintf(file, "LIGHTS %d %d %d \"%s\" %d %d ", 
        state->currentPhaseIndex, 
        state->phaseTimer, 
        state->emergencyActive ? 1 : 0,
        state->currentLightState.name,
        state->currentLightState.greenDuration,
        state->currentLightState.yellowDuration);

        for (int d = 0; d < DIR_COUNT; d++) {
            for (int t = 0; t < TURN_COUNT; t++) {
                fprintf(file, "%d ", state->currentLightState.greenMatrix[d][t] ? 1 : 0);
            }
        }      
    fprintf(file, "\n");
    
    // execution mode
    fprintf(file, "EXEC %d %d\n",
        state->executionMode,
        state->frameInStep);

    // active passing vehicles
    fprintf(file, "CROSSING %d\n", state->activeCrossingCount);
    for (int i = 0; i < state->activeCrossingCount; i++) {
        const ActiveCrossingVehicle *active = &state->crossings[i];
        fprintf(file, "VEH %d %d %d %d %d %d %f %f\n",
        active->vehicle.id,
        active->vehicle.arrivalStep,
        active->vehicle.sideEntered,
        active->vehicle.turn,
        active->vehicle.isEmergency ? 1 : 0,
        active->sourceLane,
        active->progress,
        active->speedPerFrame);
    }

    // status banner
    fprintf(file, "BANNER %d \"%s\" \n",
        state->remainingFramesOfStatusBanner,
        state->statusBanner);

    // lanes
    fprintf(file, "LANES %d %d %d %d\n",
            state->approaches[NORTH].laneCount,
            state->approaches[SOUTH].laneCount,
            state->approaches[EAST].laneCount,
            state->approaches[WEST].laneCount);

    for (int d = 0; d < DIR_COUNT; d++) {
        const Approach *app = &state->approaches[d];
        for (int l = 0; l < app->laneCount; l++) {
            const LaneQueue *q = &app->lanes[l];

            fprintf(file, "QUEUE %d %d %d %d %d %d %d\n",
                    d,
                    l,
                    q->count,
                    q->maxCount,
                    q->allowedTurns[STRAIGHT] ? 1 : 0,
                    q->allowedTurns[RIGHT] ? 1 : 0,
                    q->allowedTurns[LEFT] ? 1 : 0);

            for (const VehicleNode *curr = q->front; curr != NULL; curr = curr->next) {
                fprintf(file, "VEHICLE %d %d %d %d %d\n",
                        curr->vehicle.id,
                        curr->vehicle.arrivalStep,
                        (int)curr->vehicle.sideEntered,
                        (int)curr->vehicle.turn,
                        curr->vehicle.isEmergency ? 1 : 0);
            }
        }
    }

    // scenario
    fprintf(file, "SCENARIO %zu %zu %zu\n",
        state->scenarioQueue.count,
        state->scenarioQueue.capacity,
        state->scenarioQueue.nextIndex);
    for (size_t i = 0; i < state->scenarioQueue.count; i++) {
        const ScenarioEvent* event = &state->scenarioQueue.events[i];
        fprintf(file, "EVENT %d %d %d %d\n",
        event->arrivalStep,
        event->sideEntered,
        event->turn,
        event->isEmergency ? 1 : 0);
    }
    
    // config
    fprintf(file, "CONFIG %d %d %d %d %u",
        state->simulationConfig.phaseCount,
        state->simulationConfig.maxSteps,
        state->simulationConfig.framesPerStep,
        state->simulationConfig.rngEnabled ? 1 : 0,
        state->simulationConfig.rngSeed);
    for (int i = 0; i < DIR_COUNT; i++){
        fprintf(file, " %.17g",
            state->simulationConfig.arrivalProbability[i]);
    }
    fprintf(file, "\n");
    for (int j = 0; j < state->simulationConfig.phaseCount; j++) {
        SignalPhase* phase = &state->simulationConfig.phases[j];
        fprintf(file, "PHASE \"%s\" %d %d",
            phase->name,
            phase->greenDuration,
            phase->yellowDuration);

        for (int d = 0; d < DIR_COUNT; d++) {
            for (int t = 0; t < TURN_COUNT; t++) {
                fprintf(file, " %d", phase->greenMatrix[d][t] ? 1 : 0);
            }
        }
        fprintf(file, "\n");
    }
    fprintf(file, "\n");

    // Statistics
    fprintf(file, "STATS %d",
        state->simulationStatistics.totalVehiclesPassed);
    for (int i = 0; i < DIR_COUNT; i++) {
        fprintf(file, " %d", 
            state->simulationStatistics.vehiclesPassedPerApproach[i]);
    }
    fprintf(file, " %ld", 
        state->simulationStatistics.totalWaitingSteps);
    for (int j = 0; j < DIR_COUNT; j++) {
        fprintf(file, " %d", 
            state->simulationStatistics.maxCountPerApproach[j]);
    }   
    fprintf(file, "\n");

    if (fclose(file) != 0) {
        if (errorBuffer && errorSize > 0) {
            snprintf(errorBuffer, errorSize, "ERROR: Cannot close file '%s'!", path);        // display proper error
        }
        return false;
    }

    return true;
}


bool io_load_state(const char* path, SimulationState* state, char* errorBuffer, size_t errorSize) {
    if (path == NULL || state == NULL) {
        if (errorBuffer != NULL && errorSize > 0) {
            snprintf(errorBuffer, errorSize, "ERROR: NULL pointer passed to loader!");
        }
        return false;
    }

    FILE* file = fopen(path, "r");
    if (file == NULL) {
        if (errorBuffer != NULL && errorSize > 0) {
            snprintf(errorBuffer, errorSize, "ERROR: Cannot open file '%s'!", path);
        }
        return false;
    }

    SimulationState temp = {0};
    int emergency;
    char tag[16];

    // current step and next vehicle id
    if (fscanf(file, " STEP %d %d", &temp.currentStep, &temp.nextVehicleId) != 2 ||
        temp.currentStep < 0 || 
        temp.nextVehicleId < 0) {

        return io_state_load_error(file, &temp, errorBuffer, errorSize, "Invalid STEP record in state file!");
    }

    // lights
    if (fscanf(file, " LIGHTS %d %d %d", &temp.currentPhaseIndex, &temp.phaseTimer, &emergency) != 3 ||
        emergency < 0 || emergency > 1 ||
        !read_quoted_string(file, temp.currentLightState.name, sizeof(temp.currentLightState.name)) ||
        fscanf(file, " %d %d", &temp.currentLightState.greenDuration, &temp.currentLightState.yellowDuration) != 2) {

        return io_state_load_error(file, &temp, errorBuffer, errorSize, "Invalid LIGHTS record in state file!");
    }
    temp.emergencyActive = emergency != 0;
    for (int d = 0; d < DIR_COUNT; d++) {
        for (int t = 0; t < TURN_COUNT; t++) {
            int green;
            if (fscanf(file, " %d", &green) != 1 || green < 0 || green > 1) {

                return io_state_load_error(file, &temp, errorBuffer, errorSize, "Invalid light matrix in state file!");
            }
            temp.currentLightState.greenMatrix[d][t] = green != 0;
        }
    }
    if (temp.currentPhaseIndex < 0 || 
        temp.currentPhaseIndex >= MAX_PHASES ||
        temp.phaseTimer < 0 || 
        temp.currentLightState.greenDuration < 0 ||
        temp.currentLightState.yellowDuration < 0) {
            
        return io_state_load_error(file, &temp, errorBuffer, errorSize, "Invalid LIGHTS values in state file!");
    }

    // execution mode
    int executionMode;
    if (fscanf(file, " EXEC %d %d", &executionMode, &temp.frameInStep) != 2 ||
        executionMode < EXEC_STOPPED || executionMode > EXEC_AUTO || 
        temp.frameInStep < 0) {

        return io_state_load_error(file, &temp, errorBuffer, errorSize, "Invalid EXEC record in state file!");
    }
    temp.executionMode = (ExecutionMode)executionMode;

    // active passing vehicles
    if (fscanf(file, " CROSSING %d", &temp.activeCrossingCount) != 1 ||
        temp.activeCrossingCount < 0 || temp.activeCrossingCount > MAX_ACTIVE_CROSSINGS) {

        return io_state_load_error(file, &temp, errorBuffer, errorSize, "Invalid CROSSING record in state file!");
    }
    for (int i = 0; i < temp.activeCrossingCount; i++) {
        ActiveCrossingVehicle* crossing = &temp.crossings[i];
        int direction;
        int turn;
        if (fscanf(file, " VEH %d %d %d %d %d %d %f %f",
                    &crossing->vehicle.id, &crossing->vehicle.arrivalStep, &direction, &turn,
                    &emergency, &crossing->sourceLane, &crossing->progress,
                    &crossing->speedPerFrame) != 8 ||
            direction < NORTH || direction >= DIR_COUNT || 
            turn < STRAIGHT || turn >= TURN_COUNT ||
            emergency < 0 || emergency > 1 || 
            crossing->vehicle.id < 0 || crossing->vehicle.arrivalStep < 0 || 
            crossing->sourceLane < 0 ||
            !(crossing->progress >= 0.0f && crossing->progress <= 1.0f) ||
            !(crossing->speedPerFrame >= 0.0f && crossing->speedPerFrame <= FLT_MAX)) {

            return io_state_load_error(file, &temp, errorBuffer, errorSize, "Invalid crossing vehicle in state file!");
        }
        crossing->vehicle.sideEntered = (Direction)direction;
        crossing->vehicle.turn = (TurnIntent)turn;
        crossing->vehicle.isEmergency = emergency != 0;
    }

    // status banner
    if (fscanf(file, " BANNER %d", &temp.remainingFramesOfStatusBanner) != 1 ||
        temp.remainingFramesOfStatusBanner < 0 ||
        !read_quoted_string(file, temp.statusBanner, sizeof(temp.statusBanner))) {

        return io_state_load_error(file, &temp, errorBuffer, errorSize, "Invalid BANNER record in state file!");
    }

    // lane configuration and queued vehicles
    int laneCounts[DIR_COUNT];
    if (fscanf(file, " LANES %d %d %d %d", &laneCounts[NORTH], &laneCounts[SOUTH],
               &laneCounts[EAST], &laneCounts[WEST]) != DIR_COUNT) {

        return io_state_load_error(file, &temp, errorBuffer, errorSize, "Invalid LANES record in state file!");
    }
    for (int d = 0; d < DIR_COUNT; d++) {
        if (laneCounts[d] < 1 || laneCounts[d] > MAX_LANES_PER_APPROACH) {

            return io_state_load_error(file, &temp, errorBuffer, errorSize, "Invalid lane count in state file!");
        }
        Approach* approach = &temp.approaches[d];
        approach->direction = (Direction)d;
        approach->laneCount = laneCounts[d];
        approach->lanes = calloc((size_t)approach->laneCount, sizeof(LaneQueue));
        if (approach->lanes == NULL) {

            return io_state_load_error(file, &temp, errorBuffer, errorSize, "Memory allocation failed while loading lanes!");
        }
    }

    for (int d = 0; d < DIR_COUNT; d++) {
        Approach* approach = &temp.approaches[d];
        for (int l = 0; l < approach->laneCount; l++) {
            LaneQueue* queue = &approach->lanes[l];
            int direction;
            int lane;
            int count;
            int maxCount;
            int allowed[TURN_COUNT];
            if (fscanf(file, " %15s %d %d %d %d %d %d", tag, &direction, &lane,
                       &count, &maxCount, &allowed[STRAIGHT], &allowed[RIGHT]) != 7 ||
                strcmp(tag, "QUEUE") != 0 ||
                fscanf(file, " %d", &allowed[LEFT]) != 1 ||
                direction != d || lane != l || count < 0 || maxCount < count) {
                return io_state_load_error(file, &temp, errorBuffer, errorSize, "Invalid QUEUE record in state file!");
            }
            for (int t = 0; t < TURN_COUNT; t++) {
                if (allowed[t] < 0 || allowed[t] > 1) {
                    return io_state_load_error(file, &temp, errorBuffer, errorSize, "Invalid lane turn permission in state file!");
                }
                queue->allowedTurns[t] = allowed[t] != 0;
            }
            for (int i = 0; i < count; i++) {
                Vehicle vehicle;
                int vehicleDirection;
                int turn;
                if (fscanf(file, " %15s %d %d %d %d %d", tag, &vehicle.id, &vehicle.arrivalStep, &vehicleDirection, &turn, &emergency) != 6 ||
                    strcmp(tag, "VEHICLE") != 0 ||
                    vehicleDirection < NORTH || vehicleDirection >= DIR_COUNT ||
                    turn < STRAIGHT ||  turn >= TURN_COUNT || 
                    emergency < 0 ||  emergency > 1 ||
                    vehicle.id < 0 ||  vehicle.arrivalStep < 0) {

                    return io_state_load_error(file, &temp, errorBuffer, errorSize, "Invalid queued vehicle in state file!");
                }
                vehicle.sideEntered = (Direction)vehicleDirection;
                vehicle.turn = (TurnIntent)turn;
                vehicle.isEmergency = emergency != 0;
                if (!queue_push(queue, vehicle)) {

                    return io_state_load_error(file, &temp, errorBuffer, errorSize, "Memory allocation failed while loading vehicles!");
                }
            }
            queue->maxCount = maxCount;
        }
    }
// ------------------------------------------------------------------------------------------------------ !
    // scenario schedule
    size_t scenarioCount;
    size_t scenarioCapacity;
    size_t nextIndex;
    if (fscanf(file, " SCENARIO %zu %zu %zu", &scenarioCount, &scenarioCapacity, &nextIndex) != 3 ||
        scenarioCapacity < scenarioCount || nextIndex > scenarioCount ||
        scenarioCapacity > SIZE_MAX / sizeof(ScenarioEvent)) {
        return io_state_load_error(file, &temp, errorBuffer, errorSize,
                                   "Invalid SCENARIO record in state file!");
    }
    temp.scenarioQueue.count = scenarioCount;
    temp.scenarioQueue.capacity = scenarioCapacity;
    temp.scenarioQueue.nextIndex = nextIndex;
    if (scenarioCapacity > 0) {
        temp.scenarioQueue.events = malloc(scenarioCapacity * sizeof(ScenarioEvent));
        if (temp.scenarioQueue.events == NULL) {
            return io_state_load_error(file, &temp, errorBuffer, errorSize,
                                       "Memory allocation failed while loading scenario!");
        }
    }
    for (size_t i = 0; i < scenarioCount; i++) {
        int direction;
        int turn;
        if (fscanf(file, " %15s %d %d %d %d", tag, &temp.scenarioQueue.events[i].arrivalStep,
                   &direction, &turn, &emergency) != 5 ||
            strcmp(tag, "EVENT") != 0 || temp.scenarioQueue.events[i].arrivalStep < 0 ||
            direction < NORTH || direction >= DIR_COUNT || turn < STRAIGHT ||
            turn >= TURN_COUNT || emergency < 0 || emergency > 1) {
            return io_state_load_error(file, &temp, errorBuffer, errorSize,
                                       "Invalid scenario event in state file!");
        }
        temp.scenarioQueue.events[i].sideEntered = (Direction)direction;
        temp.scenarioQueue.events[i].turn = (TurnIntent)turn;
        temp.scenarioQueue.events[i].isEmergency = emergency != 0;
    }

    // simulation configuration
    if (fscanf(file, " CONFIG %d %d %d %d %u",
               &temp.simulationConfig.phaseCount, &temp.simulationConfig.maxSteps,
               &temp.simulationConfig.framesPerStep, &emergency,
               &temp.simulationConfig.rngSeed) != 5 ||
        temp.simulationConfig.phaseCount < 0 || temp.simulationConfig.phaseCount > MAX_PHASES ||
        temp.simulationConfig.maxSteps < 0 || temp.simulationConfig.framesPerStep < 0 ||
        emergency < 0 || emergency > 1) {
        return io_state_load_error(file, &temp, errorBuffer, errorSize, "Invalid CONFIG record in state file!");
    }
    temp.simulationConfig.rngEnabled = emergency != 0;
    for (int d = 0; d < DIR_COUNT; d++) {
        if (fscanf(file, " %lf", &temp.simulationConfig.arrivalProbability[d]) != 1 ||
            !(temp.simulationConfig.arrivalProbability[d] >= 0.0 &&
              temp.simulationConfig.arrivalProbability[d] <= 1.0)) {
            return io_state_load_error(file, &temp, errorBuffer, errorSize, "Invalid arrival probability in state file!");
        }
    }

    for (int i = 0; i < temp.simulationConfig.phaseCount; i++) {
        SignalPhase* phase = &temp.simulationConfig.phases[i];
        if (fscanf(file, " %15s", tag) != 1 || strcmp(tag, "PHASE") != 0 ||
            !read_quoted_string(file, phase->name, sizeof(phase->name)) ||
            fscanf(file, " %d %d", &phase->greenDuration, &phase->yellowDuration) != 2 ||
            phase->greenDuration < 0 || phase->yellowDuration < 0) {
            return io_state_load_error(file, &temp, errorBuffer, errorSize, "Invalid PHASE record in state file!");
        }
        for (int d = 0; d < DIR_COUNT; d++) {
            for (int t = 0; t < TURN_COUNT; t++) {
                int green;
                if (fscanf(file, " %d", &green) != 1 || green < 0 || green > 1) {
                    return io_state_load_error(file, &temp, errorBuffer, errorSize, "Invalid phase light matrix in state file!");
                }
                phase->greenMatrix[d][t] = green != 0;
            }
        }
    }

    // statistics
    if (fscanf(file, " STATS %d", &temp.simulationStatistics.totalVehiclesPassed) != 1 ||
        temp.simulationStatistics.totalVehiclesPassed < 0) {
        return io_state_load_error(file, &temp, errorBuffer, errorSize,
                                   "Invalid STATS record in state file!");
    }
    for (int d = 0; d < DIR_COUNT; d++) {
        if (fscanf(file, " %d", &temp.simulationStatistics.vehiclesPassedPerApproach[d]) != 1 ||
            temp.simulationStatistics.vehiclesPassedPerApproach[d] < 0) {
            return io_state_load_error(file, &temp, errorBuffer, errorSize,
                                       "Invalid statistics in state file!");
        }
    }
    if (fscanf(file, " %ld", &temp.simulationStatistics.totalWaitingSteps) != 1 ||
        temp.simulationStatistics.totalWaitingSteps < 0) {
        return io_state_load_error(file, &temp, errorBuffer, errorSize,
                                   "Invalid waiting-step statistic in state file!");
    }
    for (int d = 0; d < DIR_COUNT; d++) {
        if (fscanf(file, " %d", &temp.simulationStatistics.maxCountPerApproach[d]) != 1 ||
            temp.simulationStatistics.maxCountPerApproach[d] < 0) {
            return io_state_load_error(file, &temp, errorBuffer, errorSize,
                                       "Invalid maximum-count statistic in state file!");
        }
    }
    int trailingCharacter;
    do {
        trailingCharacter = fgetc(file);
    } while (trailingCharacter != EOF && isspace((unsigned char)trailingCharacter));
    if (trailingCharacter != EOF || ferror(file)) {
        return io_state_load_error(file, &temp, errorBuffer, errorSize,
                                   "Unexpected data after STATS record!");
    }
    for (int i = 0; i < temp.activeCrossingCount; i++) {
        if (temp.crossings[i].sourceLane >=
            temp.approaches[temp.crossings[i].vehicle.sideEntered].laneCount) {
            return io_state_load_error(file, &temp, errorBuffer, errorSize,
                                       "Invalid crossing source lane in state file!");
        }
    }

    if (fclose(file) != 0) {
        if (errorBuffer != NULL && errorSize > 0) {
            snprintf(errorBuffer, errorSize, "ERROR: Cannot close state file!");
        }
        io_free_loaded_state(&temp);
        return false;
    }

    // replace the existing state
    io_free_loaded_state(state);
    *state = temp;
    if (errorBuffer != NULL && errorSize > 0) {
        errorBuffer[0] = '\0';
    }
    return true;
}