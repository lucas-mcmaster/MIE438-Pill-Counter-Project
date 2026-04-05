/*
 * BeamSensorHandler.c
 *
 *
 */

#include "BeamSensorHandler.h"

//counter function
void Sensor_HandlePillDrop(void) {

    if (SystemStatus.currentState == STATE_RUNNING) {
        unsigned int current_time = HAL_GetTick();

        //Debounce check to avoid double counting
        if ((current_time - SystemStatus.lastPillTime) > SystemStatus.filterDelayMs) {
            SystemStatus.currentPillCount++;
            SystemStatus.lastPillTime = current_time;
        }
    }
}
