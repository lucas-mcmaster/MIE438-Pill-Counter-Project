/*
 * UserInterface.c
 *
 *
 */

#include "UserInterface.h"
#include <stdio.h>
#include <string.h>

extern I2C_HandleTypeDef hi2c1;
extern TIM_HandleTypeDef htim3;

#define LCD_ADDR (0x27 << 1)

static uint16_t last_encoder_count = 0;
static SystemState_t last_displayed_state = STATE_ERROR;
static uint32_t last_displayed_target = 0;
static uint32_t last_displayed_current = 0;

// FIX: Added debounce tracking variable
static uint32_t last_button_press_time = 0;

// ==============================================================================
// I2C LCD DRIVER FUNCTIONS (PCF8574 Backpack)
// ==============================================================================

// FIX: New helper function exclusively for the 4-bit bootup sequence
static void lcd_send_nibble(char nibble) {
    uint8_t data_t[2];
    data_t[0] = nibble | 0x0C;  // EN=1, RS=0, Backlight=1
    data_t[1] = nibble | 0x08;  // EN=0, RS=0, Backlight=1
    HAL_I2C_Master_Transmit(&hi2c1, LCD_ADDR, data_t, 2, 100);
}

static void lcd_send_cmd(char cmd) {
    char data_u, data_l;
    uint8_t data_t[4];
    data_u = (cmd & 0xF0);
    data_l = ((cmd << 4) & 0xF0);

    data_t[0] = data_u | 0x0C;
    data_t[1] = data_u | 0x08;
    data_t[2] = data_l | 0x0C;
    data_t[3] = data_l | 0x08;
    HAL_I2C_Master_Transmit(&hi2c1, LCD_ADDR, data_t, 4, 100);
}

static void lcd_send_data(char data) {
    char data_u, data_l;
    uint8_t data_t[4];
    data_u = (data & 0xF0);
    data_l = ((data << 4) & 0xF0);

    data_t[0] = data_u | 0x0D;
    data_t[1] = data_u | 0x09;
    data_t[2] = data_l | 0x0D;
    data_t[3] = data_l | 0x09;
    HAL_I2C_Master_Transmit(&hi2c1, LCD_ADDR, data_t, 4, 100);
}

static void lcd_clear(void) {
    lcd_send_cmd(0x01);
    HAL_Delay(2);
}

static void lcd_set_cursor(int row, int col) {
    switch (row) {
        case 0: col |= 0x80; break;
        case 1: col |= 0xC0; break;
    }
    lcd_send_cmd(col);
}

static void lcd_send_string(char *str) {
    while (*str) {
        lcd_send_data(*str++);
    }
}

// ==============================================================================
// PUBLIC UI FUNCTIONS
// ==============================================================================

void UI_Init(void) {
    // FIX: Hardware-compliant HD44780 initialization sequence
    HAL_Delay(50);
    lcd_send_nibble(0x30); HAL_Delay(5);
    lcd_send_nibble(0x30); HAL_Delay(1);
    lcd_send_nibble(0x30); HAL_Delay(10);
    lcd_send_nibble(0x20); HAL_Delay(10);

    // Now safely in 4-bit mode, standard commands will work
    lcd_send_cmd(0x28);
    lcd_send_cmd(0x0C);
    lcd_clear();

    HAL_TIM_Encoder_Start(&htim3, TIM_CHANNEL_ALL);
    last_encoder_count = __HAL_TIM_GET_COUNTER(&htim3);
}

void UI_HandleButtonPress(void) {
    // FIX: Software Debounce (Ignore rapid phantom triggers)
    uint32_t current_time = HAL_GetTick();
    if ((current_time - last_button_press_time) < 50) {
        return;
    }
    last_button_press_time = current_time;

    if (SystemStatus.currentState == STATE_IDLE) {
        SystemStatus.currentPillCount = 0;
        SystemStatus.currentState = STATE_RUNNING;
    }
    else if (SystemStatus.currentState == STATE_COMPLETE) {
        SystemStatus.currentPillCount = 0;
        SystemStatus.currentState = STATE_IDLE;
    }
}

void UI_Update(void) {
    char buffer[32];

    // --- 1. Read the Rotary Encoder ---
    if (SystemStatus.currentState == STATE_IDLE) {
        uint16_t current_count = __HAL_TIM_GET_COUNTER(&htim3);
        int16_t delta = (int16_t)(current_count - last_encoder_count);

        if (delta >= 4 || delta <= -4) {
            int16_t clicks = delta / 4;

            if (clicks > 0) {
                SystemStatus.targetPillCount += clicks;
            } else if (clicks < 0) {
                uint32_t decrement = (uint32_t)(-clicks);
                if (SystemStatus.targetPillCount > decrement) {
                    SystemStatus.targetPillCount -= decrement;
                } else {
                    SystemStatus.targetPillCount = 1; // Floor it at 1
                }
            }
            last_encoder_count += (clicks * 4);
        }
    }

    // --- 2. Take a Snapshot of Volatiles (Fixes Race Condition) ---
    // We grab these exactly once per loop so they cannot change underneath us.
    SystemState_t snap_state = SystemStatus.currentState;
    uint32_t snap_target = SystemStatus.targetPillCount;
    uint32_t snap_current = SystemStatus.currentPillCount;

    // --- 3. State-Based Screen Refresh ---
    // Use the snapshots for all comparisons
    if (snap_state != last_displayed_state ||
        snap_target != last_displayed_target ||
        snap_current != last_displayed_current) {

        switch (snap_state) {

            case STATE_IDLE:
                lcd_set_cursor(0, 0);
                lcd_send_string("Ready to Count  ");

                lcd_set_cursor(1, 0);
                sprintf(buffer, "Target: %lu", snap_target);
                break;

            case STATE_RUNNING:
                lcd_set_cursor(0, 0);
                lcd_send_string("Counting...     ");

                lcd_set_cursor(1, 0);
                sprintf(buffer, "%lu / %lu", snap_current, snap_target);
                break;

            case STATE_COMPLETE:
                lcd_set_cursor(0, 0);
                lcd_send_string("DONE!           ");

                lcd_set_cursor(1, 0);
                sprintf(buffer, "Pills: %lu", snap_current);
                break;

            default:
                buffer[0] = '\0'; // Empty string for safety
                break;
        }

        // FIX: Hardware-safe string padding to exactly 16 characters
        // This ensures old characters are erased without causing LCD overruns
        if (buffer[0] != '\0') {
            int len = strlen(buffer);
            while (len < 16) {
                buffer[len] = ' ';
                len++;
            }
            buffer[16] = '\0'; // Cap it cleanly

            lcd_send_string(buffer);
        }

        // Update trackers with our snapshots
        last_displayed_state = snap_state;
        last_displayed_target = snap_target;
        last_displayed_current = snap_current;
    }
}
