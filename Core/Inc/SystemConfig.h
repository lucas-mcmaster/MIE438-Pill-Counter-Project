/*
 * SystemConfig.h
 *
 *
 */

#ifndef INC_SYSTEMCONFIG_H_
#define INC_SYSTEMCONFIG_H_

//enumeration for all states. Can add onto this as needed
typedef enum {
    STATE_IDLE,       //State on start-up - waiting for pharmacist to turn the dial and press start
    STATE_RUNNING,    //Counting state - motor is spinning, counting pills
    STATE_COMPLETE,   //Target reached, motor stopped, waiting for reset
    STATE_ERROR       //Error state to handle any issues
} SystemState_t; //using _t to represent that its a data type

//Global system struct - we can add onto this as needed
typedef struct {
    SystemState_t currentState;

    unsigned int targetPillCount;  //pill count set by the user set via the rotary encoder
    unsigned int currentPillCount; //how many pills the sensor has seen

    unsigned int filterDelayMs;    //debounce time for the IR sensor
    unsigned int lastPillTime;     //timestamp of the last detected pill
} SystemConfig_t;

//initializing the struct with extern so that its global and volatile to prevent  compiler issues
extern volatile SystemConfig_t SystemStatus;

//public functions
void System_Init(void);
void System_ProcessState(void);

#endif /* INC_SYSTEMCONFIG_H_ */
