/*
 * MotorDriver.c
 *
 */

//including main for pin macros
#include "MotorDriver.h"
#include "main.h"

//Motor driver cannot use HAL delay as it stops CPU from doing anything else
//using static variables to keep them private to source file
static unsigned int last_step_time=0; //variable to store time of last step for speed/rotation control
static unsigned int current_step=0;

//not static so can be adjusted in other codes if needed
unsigned int motor_speed_delay=5; //motor speed delay (motor step every 5 ms--can change)

//stepper sequence to rotate the stepper in the correct order
static const uint8_t step_sequence[4][4] = {{1,0,1,0}, //step 0 (Pin1,Pin2,Pin3,Pin4)
											{0,1,1,0}, //step 1
											{0,1,0,1}, //step 2
											{1,0,0,1}}; //step 3

//helper function to set motor pins at each step
static void Set_Motor_Pins(uint8_t step)
{
	HAL_GPIO_WritePin(MOTOR_OUT_1_GPIO_Port, MOTOR_OUT_1_Pin, step_sequence[step][0] ? GPIO_PIN_SET : GPIO_PIN_RESET); //if step_sequence set to 1 GPIO_PIN_SET (high) is used if false then reset (low)
	HAL_GPIO_WritePin(MOTOR_OUT_2_GPIO_Port, MOTOR_OUT_2_Pin, step_sequence[step][1] ? GPIO_PIN_SET : GPIO_PIN_RESET);
	HAL_GPIO_WritePin(MOTOR_OUT_3_GPIO_Port, MOTOR_OUT_3_Pin, step_sequence[step][2] ? GPIO_PIN_SET : GPIO_PIN_RESET);
	HAL_GPIO_WritePin(MOTOR_OUT_4_GPIO_Port, MOTOR_OUT_4_Pin, step_sequence[step][3] ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

//Motor stop function
void Motor_Stop(void)
{
	//sets all pins to low to stop motion instantly. for when pill counter reaches required amount
	HAL_GPIO_WritePin(MOTOR_OUT_1_GPIO_Port, MOTOR_OUT_1_Pin, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(MOTOR_OUT_2_GPIO_Port, MOTOR_OUT_2_Pin, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(MOTOR_OUT_3_GPIO_Port, MOTOR_OUT_3_Pin, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(MOTOR_OUT_4_GPIO_Port, MOTOR_OUT_4_Pin, GPIO_PIN_RESET);
}

//Motor initialization function
void Motor_Init(void)
{
    Motor_Stop(); //to ensure start up occurs with no power to motor
    current_step = 0; //resetting current step to 0
    last_step_time = HAL_GetTick(); //to get first last step time in milliseconds
}

void Motor_Update(void)
{
	//safety check to only spin motor when the system state is set to running
	if(SystemStatus.currentState != STATE_RUNNING) //CHANGE THIS ONCE SYSTEM STATE IS WRITTEN
	{
		Motor_Stop(); //stops motor if not in correct state
		return;
	}

	//timer for stepper
	unsigned int current_time = HAL_GetTick();

	//if statement to move onto next step in motor
	if((current_time-last_step_time) >= motor_speed_delay)
	{
		last_step_time = current_time; //reseting timer

		Set_Motor_Pins(current_step); //calling function to move motor

		current_step++;
		//setting current step back to 0 if it is at 3
		if(current_step>3)
		{
			current_step=0;
		}

	}

	return;

}


