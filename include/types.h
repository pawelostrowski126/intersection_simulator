#ifndef TYPES_H
#define TYPES_H
#include <stddef.h>
#include <stdbool.h>

// ----- LIMITS, DEFAULTS, CONSTANTS -----

// intersection/lanes configuration
#define MAX_LANES_PER_APPROACH 4            // maximum number of lanes for each approach
#define DEFAULT_LANES 1                     // default number of lanes for each approach

// lights phases configuration
#define MAX_PHASES 6                        // maximum number of light phases
#define PHASE_NAME_LEN 16                   // maximum lenght of light phase's name
#define DEFAULT_GREEN_DURATION 5            // default green duration
#define DEFAULT_YELLOW_DURATION 2           // default yellow duration

// steps/probability configuration
#define DEFAULT_MAX_STEPS 100               // default max number of step
#define DEFALUT_RNG_SEED 42                 // default rng seed
#define DEFAULT_SPAWN_PROBABILITY 0.20      // default vehicle spawn probability for each lane
#define DEFAULT_CHANCE_OF_EMERGENCY 1       // default chance to spawn emergency vehicle
 
// animation/rendering configuration
#define FRAME_TIME_MS 30                    // refresh rate
#define DEFAULT_FRAMES_PER_STEP 10          // default number of frames per step
#define MIN_FRAMES_PER_STEP 3               // minimal value for frames per step
#define MAX_FRAMES_PER_STEP 60              // maximal value for frames per step
#define MAX_ACTIVE_CROSSINGS 32             // maximal amount of vehicles crossing
#define STATUS_MSG_LEN 128                  // maximum lenght of banner notification


// ----- ENUMERATIONS -----

typedef enum Direction {
    NORTH = 0,
    SOUTH = 1,
    EAST = 2,
    WEST = 3,
    DIR_COUNT = 4
} Direction;

typedef enum TurnIntent {
    STRAIGHT = 0,
    RIGHT = 1,
    LEFT = 2,
    TURN_COUNT = 3
} TurnIntent;

typedef enum LightColor {
    GREEN = 0,
    YELLOW = 1,
    RED = 2
} LightColor;

typedef enum ExecutionMode {
    EXEC_STOPPED = 0,   // simulation terminated
    EXEC_PAUSED = 1,    // simulation frozen mid-frame; accepts step, config, save
    EXEC_STEP = 2,      // advances exactly one logical step, then reverts to paused
    EXEC_AUTO = 3       // continous execution
} ExecutionMode;


// ----- CORE ENTITIES -----

typedef struct Vehicle {
    int id;
    int arrivalStep;
    Direction sideEntered;
    TurnIntent turn;
    bool isEmergency;
} Vehicle;

typedef struct VehicleNode {
    Vehicle vehicle;
    struct VehicleNode* next;  // next car in the node
} VehicleNode;

typedef struct LaneQueue {
    VehicleNode* front;
    VehicleNode* rear;
    int count;                      // number of vehicles in current lane
    int maxCount;                  // peak count
    bool allowedTurns[TURN_COUNT];  // maneuvers permitted from this lane;
} LaneQueue;

typedef struct Approach {
    Direction direction;        // direction of certain approach
    int laneCount;              // number of lanes in certain approach
    LaneQueue* lanes;           // array of size laneCount
} Approach;

typedef struct ActiveCrossingVehicle {
    Vehicle vehicle;        
    int sourceLane;         // origin direction
    float progress;         // 0-1 value; 0 - stop line, 1 - passed the intersecion
    float speedPerFrame;    // added each render frame
} ActiveCrossingVehicle;


// ----- SCENARIO SCHEDULE -----

typedef struct ScenarioEvent {
    int arrivalStep;            // target simulation step
    Direction sideEntered;
    TurnIntent turn;
    bool isEmergency;
} ScenarioEvent;

typedef struct ScenarioQueue {
    ScenarioEvent* events;  // array of events sorted by arrival step
    size_t count;           // count of events in arrav
    size_t capacity;        // allocated capacity of array
    size_t nextIndex;       // index of next event
} ScenarioQueue;


// ----- LIGHTS PHASES AND SIMULATION CONFIGURATION -----

typedef struct SignalPhase {
    char name[PHASE_NAME_LEN];                  // phase label
    bool greenMatrix[DIR_COUNT][TURN_COUNT];    // true - green, false - red
    int greenDuration;
    int yellowDuration;
} SignalPhase;

typedef struct SimulationConfig {
    // int lanesPerApproach[DIR_COUNT];        // number of lanes for each approach
    SignalPhase phases[MAX_PHASES];         // sequence of traffic lights phases
    int phaseCount;                         // how many phases

    int maxSteps;                           // auto stop limit (0 - unlimited)
    int framesPerStep;                      // frames per logical step

    bool rngEnabled;                        // toggle randomness
    unsigned int rngSeed;                   // seed passed to srand()
    double arrivalProbability[DIR_COUNT];   // probability to spawn vehicle for each approach
} SimulationConfig;


// ----- STATISTICS AND GENERAL STATE ------

typedef struct SimulationStatistics {
    int totalVehiclesPassed;                    // number of all vehicles passed
    int vehiclesPassedPerApproach[DIR_COUNT];   // number of vehicles passed for each approach
    long totalWaitingSteps;                     // sum of waiting times for each vehicle    
    int maxCountPerApproach[DIR_COUNT];         
} SimulationStatistics;

typedef struct SimulationState {
    int currentStep;
    int nextVehicleId;

    int currentPhaseIndex;                                  // index of active light phase 
    int phaseTimer;                                         // steps elapsed in active phase
    SignalPhase currentLightState;
    bool emergencyActive;

    ExecutionMode executionMode;                            
    int frameInStep;                                        // sub-step frame counter
    ActiveCrossingVehicle crossings[MAX_ACTIVE_CROSSINGS];  // all crossing vehicles
    int activeCrossingCount;                                // number of --//--

    char statusBanner[STATUS_MSG_LEN];                      // HUD notrification text
    int remainingFramesOfStatusBanner;                      // remaining frames that current banner should be displayed

    Approach approaches[DIR_COUNT];
    ScenarioQueue scenarioQueue;
    SimulationConfig simulationConfig;
    SimulationStatistics simulationStatistics;
} SimulationState;

#endif