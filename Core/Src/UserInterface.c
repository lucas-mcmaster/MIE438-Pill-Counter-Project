/*
 * UserInterface.c
 *
 *
 */

#include "UserInterface.h"
#include "main.h"
#include <stdio.h>
#include <string.h>

extern I2C_HandleTypeDef hi2c1;
extern TIM_HandleTypeDef htim3;

#define LCD_ADDR (0x27 << 1) //LCD 7 bit address for I2C shifted for HAL

static uint16_t last_encoder_count = 0; //using 16 bit because the timer is 16bit. I learned this prevents 32 bit arithmetic in subtraction which would cause incorrect overflow
static SystemState_t last_displayed_state = STATE_ERROR; //initializing to this so will update on startup
static unsigned int last_displayed_target = 0;
static unsigned int last_displayed_current = 0;

//Added debounce tracking variable for button press of KY40
static unsigned int last_button_press_time = 0;


//I2C LCD Driver helper funcs. Using 4 bit nibbles to send data
//New helper function for the 4-bit bootup sequence. Run in INIT state to switch LCD to 4 bit mode
static void lcd_send_nibble(char nibble) { //nibble is byte needed to reset board and enable 4 bit mode
    uint8_t data_t[2];
    //sending data_t here in two packets with enable switching from 1 to 0 to switch to 4 bit mode
    data_t[0] = nibble | 0x0C;  //EN=1, RS=0, Backlight=1 RS=command mode
    data_t[1] = nibble | 0x08;  //EN=0, RS=0, Backlight=1

    if (HAL_I2C_Master_Transmit(&hi2c1, LCD_ADDR, data_t, 2, 100)!=HAL_OK) //calling function in If statement for error handling in case of I2C issues
    {
    	SystemStatus.currentState=STATE_ERROR;
    }
}

//send commands in 4 bit mode.
static void lcd_send_cmd(char cmd) {
	//data u is upper 4 bits, data_l is lower 4 bits of command
    char data_u, data_l;
    uint8_t data_t[4];
    //slicing the byte into half
    data_u = (cmd & 0xF0);
    data_l = ((cmd << 4) & 0xF0); //shift to top 4 bits and make the rest 0

    //sending the character in  pulses
    data_t[0] = data_u | 0x0C; //EN=1, RS=0, Backlight=1 RS=command mode
    data_t[1] = data_u | 0x08; //EN=0 RS=0 Backlight=1
    data_t[2] = data_l | 0x0C; //EN=1, RS=0, Backlight=1 RS=command mode
    data_t[3] = data_l | 0x08; //EN=0 RS=0 Backlight=1

    if (HAL_I2C_Master_Transmit(&hi2c1, LCD_ADDR, data_t, 4, 100)!=HAL_OK) //calling function in If statement for error handling in case of I2C issues
    {
    	SystemStatus.currentState=STATE_ERROR;
    }
}

//sending data in 4 bit packets
static void lcd_send_data(char data) {
	//data u is upper 4 bits, data_l is lower 4 bits of character
	char data_u, data_l;
    uint8_t data_t[4];
    //slicing the byte into half
    data_u = (data & 0xF0);
    data_l = ((data << 4) & 0xF0);

    data_t[0] = data_u | 0x0D; //EN=1, RS=1, Backlight=1 RS=text data mode
    data_t[1] = data_u | 0x09; //EN=0, RS=1, Backlight=1 RS=text data mode
    data_t[2] = data_l | 0x0D; //EN=1, RS=1, Backlight=1 RS=text data mode
    data_t[3] = data_l | 0x09;  //EN=0, RS=1, Backlight=1 RS=text data mode

    if (HAL_I2C_Master_Transmit(&hi2c1, LCD_ADDR, data_t, 4, 100)!=HAL_OK) //calling function in If statement for error handling in case of I2C issues
        {
        	SystemStatus.currentState=STATE_ERROR;
        }
}

//to clear LCD on initalization/reset
static void lcd_clear(void) {
    lcd_send_cmd(0x01);
    HAL_Delay(2); //delay to ensure this is done before next command sent
}

//set cursor func to choose which spot on the row to put the cursor
static void lcd_set_cursor(int row, int col) {
    switch (row) {
        case 0: col |= 0x80; break;
        case 1: col |= 0xC0; break;
    }
    lcd_send_cmd(col);
}

//function to send entire string
static void lcd_send_string(char *str) {
	//once string is done, *str will point to null terminator so while loop will stop
    while (*str) {
        lcd_send_data(*str); //sending character of string to lcd
        str++; //incrementing string pointer to next character
    }
}


//public UI functions

//LCD initialization function
void UI_Init(void) {
    //HHD44780 initialization sequence - from stackoverflow. Each startup command requires a delay before the next one can be sent
    HAL_Delay(50);
    lcd_send_nibble(0x30);
    HAL_Delay(5);
    lcd_send_nibble(0x30);
    HAL_Delay(1);
    lcd_send_nibble(0x30);
    HAL_Delay(10);
    lcd_send_nibble(0x20); //command for switching to 4 bit mode
    HAL_Delay(10);

    //Now safely in 4-bit mode sending standard commands should be fine. Initializing from HD44780 manual
    lcd_send_cmd(0x28); //Setting 4-bit, 2 lines, 5x8 font
    HAL_Delay(1);

    lcd_send_cmd(0x08); //display off (manual says to do this)
    HAL_Delay(1);

    lcd_clear();

    lcd_send_cmd(0x06); //Entry mode set increments cursor to the right. No display shift?
    HAL_Delay(1);

    //powering LCD on now
    lcd_send_cmd(0x0C); //Display on, cursor off, blink off

    //starting hardware timer for rotary encoder
    HAL_TIM_Encoder_Start(&htim3, TIM_CHANNEL_ALL);
    last_encoder_count = __HAL_TIM_GET_COUNTER(&htim3);
}

//function for button press on encorder - when interrupt occurs this is called to switch states
void UI_HandleButtonPress(void) {
    //Added a software debounce to ignore random phantom button triggers when testing
    unsigned int current_time = HAL_GetTick();
    if ((current_time - last_button_press_time) < 350) { //350 millisecond button debounce - can adjust if needed
        return;
    }
    last_button_press_time = current_time;

    if (SystemStatus.currentState == STATE_IDLE) { //if in idle set currentPillCount to 0 and switch to running state
        SystemStatus.currentPillCount = 0;

        //added this for inventory mode timeout - setting lastpilltime to Hal_GetTick so it doesn't instantly timeout with lastpill at 0
        SystemStatus.lastPillTime = HAL_GetTick();
        SystemStatus.currentState = STATE_RUNNING;
    }
    else if (SystemStatus.currentState == STATE_RUNNING)
    {
        //added this to allow user to manually stop
        //required for inventory mode but also serves as an emergency stop for regular counting mode.
        SystemStatus.currentState = STATE_COMPLETE;
    }

    else if (SystemStatus.currentState == STATE_COMPLETE) { //when done switch back to idle state to use again
        SystemStatus.currentPillCount = 0;
        SystemStatus.currentState = STATE_IDLE;
    }

    else if (SystemStatus.currentState == STATE_ERROR)
    {
        //added this to allow user to clear an error state without rebooting the STM32 - should never get into this state but just in case
        BSP_LED_Off(LED_YELLOW); //clear the error LED
        SystemStatus.currentState = STATE_IDLE;
    }
}

//function to actively update UI
void UI_Update(void) {
    char buffer[32]; //string for dynamic data sending to LCD

    // for reading rotary encoder - only works in idle state to prevent encoder affecting anything when its running
    if (SystemStatus.currentState == STATE_IDLE) {
        uint16_t current_count = __HAL_TIM_GET_COUNTER(&htim3);
        //calculating delta to determine rotational speed to find out how much to increment
        int16_t delta = (int16_t)(current_count - last_encoder_count); //hard casting to signed arithmetic to calculate the rotational speed. Shouldn't cause issues cause not possible to spin it fast enough


        //each click takes 4 ticks so we ignore delta if less than 4
        if (delta >= 4 || delta <= -4) {
            int16_t clicks = delta / 4;

            if (clicks > 0) {
                SystemStatus.targetPillCount += clicks; //increment pill count by number of clicks

            } else if (clicks < 0) {

            	//code to prevent unsigned underflow from substraction
                unsigned int decrement = (unsigned int)(-clicks); //making clicks positive and then casting it to unsigned int and assigning it to decrement
                if (SystemStatus.targetPillCount > decrement) { //allow the decrement if will not go below 0
                    SystemStatus.targetPillCount -= decrement;
                } else {
                    SystemStatus.targetPillCount = 0; //Flooring  it at 0 if user goes too far down
                }
            }
            last_encoder_count += (clicks * 4); //setting last encoder count based on clicks and not current_count to prevent drift from slight overshoot when spinning
        }
    }

    //snapshot temporary variables to fix race condition - from testing when LCD would not update properly
    //grab these exactly once per loop so they cannot change below
    SystemState_t snap_state = SystemStatus.currentState;
    unsigned int snap_target = SystemStatus.targetPillCount;
    unsigned int snap_current = SystemStatus.currentPillCount;

    //refreshing LCD screen on change only
    //using the snapshots for all comparisons to ensure correct updates
    if (snap_state != last_displayed_state ||
        snap_target != last_displayed_target ||
        snap_current != last_displayed_current) {

    	//depending on state LCD will show different things
    	//using sprintf to store string in memory
    	//potentially change wording later - make sure to stay within LCD size
        switch (snap_state) {

            case STATE_IDLE:
                lcd_set_cursor(0, 0);

                //if statement to differentiate between invenotry and target count mode
                if (snap_target == 0)
                {
                	lcd_send_string("Inventory Check ");  //inventory mode idle state  - adding spaces to clear previous text on LCD. DO NOT EXCEED 16 char
					lcd_set_cursor(1, 0);
					sprintf(buffer, "Target: NONE    "); //when target count is 0 setting this to done to reduce confusion
                }
                else
                {
					lcd_send_string("Ready to Count  "); //target count mode idle state - adding spaces to clear previous text on LCD. DO NOT EXCEED 16 char

					lcd_set_cursor(1, 0);
					sprintf(buffer, "Target: %u", snap_target); //target count - updates with encoder
                }
                break;

            case STATE_RUNNING:
                lcd_set_cursor(0, 0);
                lcd_send_string("Counting...     "); //during counting process

                lcd_set_cursor(1, 0);

                //if else statement to hide fraction if in inventory mode
                if (snap_target==0)
                {
					sprintf(buffer, "Total: %u", snap_current);
				}
                else
                {
                	sprintf(buffer, "%u / %u", snap_current, snap_target); //shows current count/target
                }

                break;

            case STATE_COMPLETE:
                lcd_set_cursor(0, 0);
                lcd_send_string("DONE!           "); //once done show pill count

                lcd_set_cursor(1, 0);
                sprintf(buffer, "Pills: %u", snap_current);
                break;

            case STATE_ERROR: //error state - attempts to print out to LCD and flashed yellow led for info
            	lcd_set_cursor(0, 0);
				lcd_send_string("ERROR!          ");

				lcd_set_cursor(1, 0);
				lcd_send_string("Check Machine   ");

				//Turning on the yellow LED to grab attention
				BSP_LED_On(LED_YELLOW);

				break;

            default:
                buffer[0] = '\0'; // Empty string for safety
                break;
        }

        //Hardware-safe string padding to exactly 16 characters. prevents big numbers causing issues
        //also ensures old characters are erased without causing LCD overruns
        if (buffer[0] != '\0') {
            int len = strlen(buffer);
            while (len < 16) {
                buffer[len] = ' ';
                len++;
            }
            buffer[16] = '\0'; //cuts off all data after 16th byte from being printed by LCD

            lcd_send_string(buffer);
        }

        //Update trackers with our snapshots
        last_displayed_state = snap_state;
        last_displayed_target = snap_target;
        last_displayed_current = snap_current;
    }
}
