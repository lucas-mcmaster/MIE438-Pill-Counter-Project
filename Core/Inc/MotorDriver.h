/*
 * MotorDriver.h
 *
 */

#ifndef INC_MOTORDRIVER_H_
#define INC_MOTORDRIVER_H_

#include "stm32h7xx_hal.h" //for HAL
#include "SystemConfig.h" //To access the system states needed

//Including all functions
void Motor_Stop(void);

void Motor_Init(void);

void Motor_Update(void);


#endif /* INC_MOTORDRIVER_H_ */
