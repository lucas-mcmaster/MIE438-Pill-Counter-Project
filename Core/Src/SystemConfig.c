/*
 * SystemConfig.c
 *
 *
 *
 */

#include "SystemConfig.h"
#include "main.h"

//intitializing the global var
volatile SystemConfig_t SystemStatus;

//system initialization function on power up

void System_Init(void) {
    //setting default startup values
    SystemStatus.currentState = STATE_IDLE;

    SystemStatus.targetPillCount = 1; //starting target at 1 and current pill count at 0
    SystemStatus.currentPillCount = 0;

    SystemStatus.filterDelayMs = 75;   //blind-time after a pill is detected to prevent double counting - CURRENTLY 20ms LOOK TO CHANGE AFTER TESTING
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
        	//Adding that target pill count > 0 for inventory mode
            if (SystemStatus.targetPillCount>0 && SystemStatus.currentPillCount >= SystemStatus.targetPillCount) //setting to greater or equal in case we overcount it doesnt lock into running state incorrectly
            {
            	SystemStatus.currentState = STATE_COMPLETE;
            }

            //else if statement to force system to complete state if no pill drops for 10 seconds - for inventory mode so user doesn't have to stop themself
            else if ((HAL_GetTick()-SystemStatus.lastPillTime) >= 10000)
            {
            	SystemStatus.currentState = STATE_COMPLETE;
            }
            break;

        case STATE_COMPLETE:
            //Once target pill count is reached. The motordriver function sees this state and immediately stops the motor, and then waiting for the user to press the button to reset back to STATE_IDLE.
            break;

        case STATE_ERROR:
            //Placeholder for future safety feature.
            break;
    }
}
