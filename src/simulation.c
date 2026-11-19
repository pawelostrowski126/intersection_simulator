#include "simulation.h"
#include "intersection.h"
#include "io.h"
#include "queue.h"

// LOCAL FUNCTIONS

static void init_default_light_phases(SimulationConfig* config) {       // initialize default light phases
    if (config == NULL) {
        return;
    }

    config->phaseCount = 2;

    SignalPhase* northSouthPhase = &config->phases[0];
    snprintf(northSouthPhase->name, PHASE_NAME_LEN, "N-S Green");
    northSouthPhase->greenDuration = DEFAULT_GREEN_DURATION;
    northSouthPhase->yellowDuration = DEFAULT_YELLOW_DURATION;
    for (int t = 0; t < TURN_COUNT; t++) {
        northSouthPhase->greenMatrix[NORTH][t] = true;
        northSouthPhase->greenMatrix[SOUTH][t] = true;
        northSouthPhase->greenMatrix[EAST][t] = false;
        northSouthPhase->greenMatrix[WEST][t] = false;
    }

    SignalPhase* eastWestPhaes = &config->phases[1];
    snprintf(eastWestPhaes->name, PHASE_NAME_LEN, "E-W Green");
    eastWestPhaes->greenDuration = DEFAULT_GREEN_DURATION;
    eastWestPhaes->yellowDuration = DEFAULT_YELLOW_DURATION;
    for (int t = 0; t < TURN_COUNT; t++) {
        eastWestPhaes->greenMatrix[NORTH][t] = false;
        eastWestPhaes->greenMatrix[SOUTH][t] = false;
        eastWestPhaes->greenMatrix[EAST][t] = true;
        eastWestPhaes->greenMatrix[WEST][t] = true;
    }
}


static void vehicle_spawn(SimulationState* state, Direction side, TurnIntent turn, bool isEmergency) {  // spawns vehicle on certain lane
    if (state == NULL) {
        return;
    }

    Vehicle newVehicle;
    newVehicle.id = state->nextVehicleId++;
    newVehicle.arrivalStep = state->currentStep;
    newVehicle.sideEntered = side;
    newVehicle.turn = turn;
    newVehicle.isEmergency = isEmergency;

    int laneIndex = intersection_select_best_lane(&state->approaches[side], turn);
    if (laneIndex < 0 || laneIndex >= state->approaches[side].laneCount) {
        laneIndex = 0; // fallback to first lane if no valid lane found
    }

    LaneQueue* targetQueue = &state->approaches[side].lanes[laneIndex];
    if (queue_push(targetQueue, newVehicle)) {
        if (targetQueue->count > targetQueue->maxCount) {   // update maxCount if queue_push succeeds
            targetQueue->maxCount = targetQueue->count;
        }

        int totalVehiclesOnApproach = 0;
        for (int l = 0; l < state->approaches[side].laneCount; l++) {
            totalVehiclesOnApproach += state->approaches[side].lanes[l].count;
        }
        if (totalVehiclesOnApproach > state->simulationStatistics.maxCountPerApproach[side]) {
            state->simulationStatistics.maxCountPerApproach[side] = totalVehiclesOnApproach;
        }
    }
}


static void step_vehicle_spawn(SimulationState* state) {    // reads scenario and spawn vehicles if needed and check if rng is enabled and also spawn vehicles
    ScenarioQueue* scenario = &state->scenarioQueue;
    while (scenario->nextIndex < scenario->count && 
           scenario->events[scenario->nextIndex].arrivalStep == state->currentStep) {

            const ScenarioEvent* event = &scenario->events[scenario->nextIndex++];
            vehicle_spawn(state, event->sideEntered, event->turn, event->isEmergency);
    }

    if (!state->simulationConfig.rngEnabled) {
        return;
    }

    for (int d = 0; d < DIR_COUNT; d++) {
        double roll = (double)rand() / (double)RAND_MAX;
        if (roll < state->simulationConfig.arrivalProbability[d]) {
            TurnIntent randomTurn = (TurnIntent)(rand() % TURN_COUNT);
            bool randomEmergency = ((rand() % 100) < DEFAULT_CHANCE_OF_EMERGENCY);
            vehicle_spawn(state, (Direction)d, randomTurn, randomEmergency);
        }
    }
}


static Direction opposing_direciton(Direction d) {      // returns opposite direction of given direciton
    switch (d) {
        case NORTH: return SOUTH;
        case SOUTH: return NORTH;
        case EAST: return WEST;
        case WEST: return EAST;
        default:    return NORTH;
    }
}


static bool check_emergency(const SimulationState* state, Direction* directionOut, TurnIntent* turnOut) {   // check if there are any emegrency vehicles in any lane
    for (int d = 0; d < DIR_COUNT; d++) {
        const Approach* approach = &state->approaches[d];
        for (int l = 0; l < approach->laneCount; l++) {
            for(VehicleNode* node = approach->lanes[l].front; node != NULL; node = node->next) {
                if (node->vehicle.isEmergency) {
                    *directionOut = (Direction)d;
                    *turnOut = node->vehicle.turn;
                    return true;
                }
            }
        }
    }

    return false;
}


static void step_update_lights(SimulationState* state) {
    SimulationConfig* config = &state->simulationConfig;
    
    if (config->phaseCount <= 0) {  // check if there are any light phases
        return;
    }

    Direction emergencyDirection = NORTH;
    TurnIntent emergencyTurn = STRAIGHT;

    if (check_emergency(state, &emergencyDirection, &emergencyTurn)) {
        for (int p = 0; p < config->phaseCount; p++) {
            if (config->phases[p].greenMatrix[emergencyDirection][emergencyTurn]) {
                if (!state->emergencyActive || state->currentPhaseIndex != p) {
                    char bannerMessage[STATUS_MSG_LEN];
                    snprintf(bannerMessage, sizeof(bannerMessage), "EMERGENCY PREEMPTION: Phase %s",
                             config->phases[p].name);
                    simulation_set_banner(state, bannerMessage, 90);
                }
                state->currentPhaseIndex = p;
                state->currentLightState = config->phases[p];
                state->phaseTimer = 0;
                state->emergencyActive = true;
                return;
            }
        }
    }

    state->emergencyActive = false;
    state->phaseTimer++;

    if (state->phaseTimer == state->currentLightState.greenDuration) {
        memset(state->currentLightState.greenMatrix, 0, sizeof(state->currentLightState.greenMatrix));
    }
    else if (state->phaseTimer >= state->currentLightState.greenDuration + state->currentLightState.yellowDuration) {
        state->currentPhaseIndex = (state->currentPhaseIndex + 1) % config->phaseCount;
        state->currentLightState = config->phases[state->currentPhaseIndex];
        state->phaseTimer = 0;
    }
}


static bool check_left_turn_blocked(const SimulationState* state, const SignalPhase* phase, Direction sideEntered) {
    Direction oppositeDirection = opposing_direciton(sideEntered);
    const Approach* oppositeApproach = &state->approaches[oppositeDirection];

    for (int l = 0; l < oppositeApproach->laneCount; l++) {
        const Vehicle* frontVehicle = queue_peek(&oppositeApproach->lanes[l]);
        if (frontVehicle != NULL && frontVehicle->turn != LEFT) {
            if (phase->greenMatrix[oppositeDirection][frontVehicle->turn]) {
                return true;
            }
        }
    }

    return false;
}


static void push_vehicle_crossing(SimulationState* state, Vehicle vehicle, int laneIndex) {
    if (state->activeCrossingCount >= MAX_ACTIVE_CROSSINGS) {
        return;
    }

    int framesPerStep = state->frameInStep;
    if (framesPerStep < 1) {
        framesPerStep = 1;
    }

    ActiveCrossingVehicle* activeVehicle = &state->crossings[state->activeCrossingCount++];
    activeVehicle->vehicle = vehicle;
    activeVehicle->sourceLane = laneIndex;
    activeVehicle->progress = 0.0f;
    activeVehicle->speedPerFrame = 1.0f / (float)framesPerStep;
}


static void step_serve_vehicles(SimulationState* state) {
    if (state->simulationConfig.phaseCount <= 0 ) {
        return;
    }

    const SignalPhase* signalPhase = &state->simulationConfig.phases[state->currentPhaseIndex];
    bool canGO[DIR_COUNT][MAX_LANES_PER_APPROACH] = {{false}};

    for (int d = 0; d < DIR_COUNT; d++) {
        const Approach* approach = &state->approaches[d];
        for (int l = 0; l < approach->laneCount; l++) {
            const Vehicle* frontVehicle = queue_peek(&approach->lanes[d]);
            if (frontVehicle == NULL) {
                continue;
            }

            bool hasGreenLight = signalPhase->greenMatrix[d][frontVehicle->turn];
            if(!hasGreenLight) {
                continue;
            }

            if(frontVehicle->turn == LEFT && !frontVehicle->isEmergency && check_left_turn_blocked(state, signalPhase, (Direction)d)) {
                continue;
            }

            canGO[d][l] = true;
        }
    }

    for (int d = 0; d < DIR_COUNT; d++) {
        const Approach* approach = &state->approaches[d];
        for (int l = 0; l < approach->laneCount; l++) {
            if(!canGO[d][l]) {
                continue;
            }

            Vehicle departingVehicle;
            if (queue_pop(&approach->lanes[l], &departingVehicle)) {
                long waitingSteps = (long)(state->currentStep - departingVehicle.arrivalStep);
                if (waitingSteps < 0) {
                    waitingSteps = 0;
                }

                state->simulationStatistics.totalVehiclesPassed++;
                state->simulationStatistics.vehiclesPassedPerApproach[d]++;
                state->simulationStatistics.totalWaitingSteps += waitingSteps;

                push_vehicle_crossing(state, departingVehicle, l);
            }
        }
    }
}


static void update_crossings(SimulationState* state) {
    int i = 0;
    while (i < state->activeCrossingCount) {
        ActiveCrossingVehicle* activeVehicle = &state->crossings[i];
        activeVehicle->progress += activeVehicle->speedPerFrame;

        if (activeVehicle->progress >= 1.0f) {
            state->crossings[i] = state->crossings[state->activeCrossingCount -1];
            state->activeCrossingCount--;
        }
        else {
            i++;
        }
    }
}


// MAIN FUNCTIONS

bool simulation_init_default(SimulationState* state) {
    // function that initialize default signal phases
    init_default_light_phases(&state->simulationConfig);

    // initialize all counters to 0
    memset(state, 0, sizeof(SimulationState));

    // step
    state->currentStep = 0;
    state->nextVehicleId = 1;

    // lights
    state->currentPhaseIndex = 0;
    state->phaseTimer = 0;
    state->currentLightState = state->simulationConfig.phases[0];
    
    // emergency
    state->emergencyActive = false;

    // execution mode
    state->executionMode = EXEC_PAUSED;
    state->frameInStep = 0;

    // crossings
    state->activeCrossingCount = 0;

    // scenario
    state->scenarioQueue.events = NULL;
    state->scenarioQueue.count = 0;
    state->scenarioQueue.capacity = 0;
    state->scenarioQueue.nextIndex = 0;

    // config
    SimulationConfig* config = &state->simulationConfig;
    config->maxSteps = DEFAULT_MAX_STEPS;
    config->framesPerStep = DEFAULT_FRAMES_PER_STEP;
    config->rngEnabled = false;
    config->rngSeed = DEFALUT_RNG_SEED;
    for (int d = 0; d < DIR_COUNT; d++) {
        config->arrivalProbability[d] = DEFAULT_SPAWN_PROBABILITY;
        state->approaches[d].laneCount = DEFAULT_LANES;
        state->approaches[d].direction = (Direction)d;
    }

    // statistics
    SimulationStatistics* statistics = &state->simulationStatistics;
    statistics->totalVehiclesPassed = 0;
    statistics->totalWaitingSteps = 0;
    for (int d = 0; d < DIR_COUNT; d++) {
        statistics->vehiclesPassedPerApproach[d] = 0;
        statistics->maxCountPerApproach[d] = 0;
    }

    // randomize seed
    if (!config->rngEnabled) {
        srand(config->rngSeed);
    }

    // initialize intersection(approaches), return false if fails
    if (!intersection_init(state)){
        return false;
    }

    snprintf(state->statusBanner, STATUS_MSG_LEN,
             "Simulation initialized. Press [SPACE] to start or [N] to step.");
    state->remainingFramesOfStatusBanner = 150;

    return true;
}


bool simulation_reset(SimulationState* state) { // !!!!! CHECK !!!!!
    if (state == NULL) {
        return false;
    }

    // step
    state->currentStep = 0;
    state->nextVehicleId = 1;

    // lights
    state->currentPhaseIndex = 0;
    state->phaseTimer = 0;
    state->currentLightState = state->simulationConfig.phases[0];
    
    // emergency
    state->emergencyActive = false;

    // execution mode
    state->executionMode = EXEC_PAUSED;
    state->frameInStep = 0;

    // crossings
    state->activeCrossingCount = 0;

    // scenario
    state->scenarioQueue.nextIndex = 0;

    // statistics
    memset(&state->simulationStatistics, 0, sizeof(SimulationStatistics));

    // approaches
    for (int d = 0; d < DIR_COUNT; d++) {
        Approach* approach = &state->approaches[d];
        if (approach->lanes != NULL){
            for (int l = 0; l < approach->laneCount; l++) {
                queue_clear(&approach->lanes[l]);
                approach->lanes[l].maxCount = 0;
            }
        }
    }

    if (state->simulationConfig.rngEnabled) {
        srand(state->simulationConfig.rngSeed);
    }

    snprintf(state->statusBanner, STATUS_MSG_LEN,
             "Simulation reset to step 0. Press [SPACE] to start or [N] to step.");
    state->remainingFramesOfStatusBanner = 150;

    return true;
}


void simulation_step(SimulationState* state) {
    if (state == NULL) {
        return;
    }

    if (state->simulationConfig.maxSteps > 0 && 
        state->currentStep >= state->simulationConfig.maxSteps) {

            state->executionMode = EXEC_PAUSED;
            return;
    }

    step_vehicle_spawn(state);
    step_update_lights(state);
    step_serve_vehicles(state);

    state->currentStep++;

    if (state->simulationConfig.maxSteps > 0 &&
        state->currentStep >= state->simulationConfig.maxSteps) {
        state->executionMode = EXEC_PAUSED;
        simulation_set_banner(state, "Max simulation steps reached.", 120);
    }

    return;
}


void simulation_frame(SimulationState* state) {
    if (state == NULL) {
        return;
    }

    if (state->remainingFramesOfStatusBanner > 0) {
        state->remainingFramesOfStatusBanner--;
        if (state->remainingFramesOfStatusBanner == 0) {
            state->statusBanner[0] = '\n';
        }
    }

    if (state->executionMode = EXEC_PAUSED) {
        return;
    }

    int framesPerStep = state->simulationConfig.framesPerStep;
    if (framesPerStep < MIN_FRAMES_PER_STEP) {
        framesPerStep = MIN_FRAMES_PER_STEP;
    }

    update_crossings(state);

    if (state->frameInStep == 0) {
        simulation_step(state);
        if (state->executionMode = EXEC_PAUSED) {
            return;
        }
    }

    state->frameInStep++;
    if (state->frameInStep >= framesPerStep) {
        state->frameInStep = 0;
        if (state->executionMode == EXEC_STEP) {
            state->executionMode = EXEC_PAUSED;
        }
    }
}


void simulation_adjust_speed(SimulationState* state, int value) {
    if (state == NULL) {
        return;
    }

    int updatedFrames = state->simulationConfig.framesPerStep + value;
    if (updatedFrames < MIN_FRAMES_PER_STEP) {
        updatedFrames = MIN_FRAMES_PER_STEP;
    }
    else if (updatedFrames > MAX_FRAMES_PER_STEP) {
        updatedFrames = MAX_FRAMES_PER_STEP;
    }

    state->simulationConfig.framesPerStep = updatedFrames;

    float updatedSpeed = 1.0f / (float)updatedFrames;
    for (int i = 0; i < state->activeCrossingCount; i++) {
        state->crossings[i].speedPerFrame = updatedSpeed;
    }

    char bannerMessage[STATUS_MSG_LEN];
    snprintf(bannerMessage, sizeof(bannerMessage), "Speed Updated: %d frames/step!", updatedFrames);
    simulation_set_banner(state, bannerMessage, 60);
}


void simulation_set_banner(SimulationState* state, const char* message, int durationFrames) {
    if (state == NULL || message == NULL) {
        return;
    }

    snprintf(state->statusBanner, STATUS_MSG_LEN, "%s", message);
    state->remainingFramesOfStatusBanner = (durationFrames > 0) ? durationFrames : 0;
}