/*
 * SystemConfig.c
 *
 *
 *
 */

#include "SystemConfig.h"

//intitializing the global var
SystemConfig_t SystemStatus;

//system initialization function on power up

void System_Init(void) {
    //setting default startup values
    SystemStatus.currentState = STATE_IDLE;

    SystemStatus.targetPillCount = 0; //starting target and current pill count at 0
    SystemStatus.currentPillCount = 0;

    SystemStatus.filterDelayMs = 20;   //blind-time after a pill is detected to prevent double counting - CURRENTLY 20ms LOOK TO CHANGE AFTER TESTING
    SystemStatus.lastPillTime = 0;     //time between pills for possible defect checking and error checking
}

void System_ProcessState(void) {
    //function to handle the high-level logic and transitions between states.
    //Place inside the main while(1) loop.

    switch (SystemStatus.currentState) {

        case STATE_IDLE:
            //In this state the motor is stopped. UI module is reading the rotary encoder to change SystemStatus.targetPillCount.
        	//If the user presses the button, the UI module will change the state to STATE_RUNNING to force movement to next state
            break;

        case STATE_RUNNING:
            //The motor is spinning and the sensor interrupt is occurring for incrementing currentPillCount.

            //Checking if the machine has met the target - we can also do this in main if we want
            if (SystemStatus.currentPillCount >= SystemStatus.targetPillCount) { //setting to greater or equal in case we overcount it doesnt lock into running state incorrectly
                SystemStatus.currentState = STATE_COMPLETE;
            }
            break;

        case STATE_COMPLETE:
            //Once target pill count is reached. The motordriver function sees this state and immediately stops the motor, and then waiting for the user to press the button to reset back to STATE_IDLE.
            break;

        case STATE_ERROR:
            // Placeholder for future safety features (e.g., motor drawing too much current).
            break;
    }
}
