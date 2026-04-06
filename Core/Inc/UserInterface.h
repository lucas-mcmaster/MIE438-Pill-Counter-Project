/*
 * UserInterface.h
 *
 *
 */

#ifndef INC_USERINTERFACE_H_
#define INC_USERINTERFACE_H_

#include "stm32h7xx_hal.h"
#include "SystemConfig.h"
//public functions for main
void UI_Init(void);
void UI_Update(void);
void UI_HandleButtonPress(void);

#endif /* INC_USERINTERFACE_H_ */
