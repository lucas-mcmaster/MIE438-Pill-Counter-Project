/*
 * UserInterface.c
 *
 * Handles LCD display, rotary encoder reading, and button press events.
 *
 * CHANGES FROM ORIGINAL:
 *  - UI_HandleButtonPress now uses System_SetState() instead of direct
 *    assignment so the stateChanged flag is always set correctly.
 *  - STATE_ERROR LCD display now shows a specific message per errorCode
 *    rather than a generic "Check Machine" message.
 *  - LCD line 2 in ERROR state cycles through error code descriptions.
 */

#include "UserInterface.h"
#include "main.h"
#include <stdio.h>
#include <string.h>

extern I2C_HandleTypeDef hi2c1;
extern TIM_HandleTypeDef htim3;

#define LCD_ADDR (0x27 << 1) // LCD 7-bit I2C address shifted for HAL

static uint16_t      last_encoder_count    = 0;
static SystemState_t last_displayed_state  = STATE_ERROR; // Forces update on startup
static unsigned int  last_displayed_target = 0;
static unsigned int  last_displayed_current = 0;
static SystemError_t last_displayed_error  = ERROR_NONE;

static unsigned int  last_button_press_time = 0;

/* =========================================================================
 * PRIVATE LCD DRIVER HELPERS (unchanged from original)
 * ========================================================================= */

static void lcd_send_nibble(char nibble)
{
    uint8_t data_t[2];
    data_t[0] = nibble | 0x0C;  // EN=1, RS=0, Backlight=1
    data_t[1] = nibble | 0x08;  // EN=0, RS=0, Backlight=1
    if (HAL_I2C_Master_Transmit(&hi2c1, LCD_ADDR, data_t, 2, 100) != HAL_OK)
    {
        /* I2C fault — set error state without overwriting a more specific code */
        if (SystemStatus.errorCode == ERROR_NONE)
            SystemStatus.errorCode = ERROR_I2C_FAULT;
        System_SetState(STATE_ERROR);
    }
}

static void lcd_send_cmd(char cmd)
{
    char    data_u, data_l;
    uint8_t data_t[4];
    data_u     = (cmd & 0xF0);
    data_l     = ((cmd << 4) & 0xF0);
    data_t[0]  = data_u | 0x0C;
    data_t[1]  = data_u | 0x08;
    data_t[2]  = data_l | 0x0C;
    data_t[3]  = data_l | 0x08;
    if (HAL_I2C_Master_Transmit(&hi2c1, LCD_ADDR, data_t, 4, 100) != HAL_OK)
    {
        if (SystemStatus.errorCode == ERROR_NONE)
            SystemStatus.errorCode = ERROR_I2C_FAULT;
        System_SetState(STATE_ERROR);
    }
}

static void lcd_send_data(char data)
{
    char    data_u, data_l;
    uint8_t data_t[4];
    data_u     = (data & 0xF0);
    data_l     = ((data << 4) & 0xF0);
    data_t[0]  = data_u | 0x0D;
    data_t[1]  = data_u | 0x09;
    data_t[2]  = data_l | 0x0D;
    data_t[3]  = data_l | 0x09;
    if (HAL_I2C_Master_Transmit(&hi2c1, LCD_ADDR, data_t, 4, 100) != HAL_OK)
    {
        if (SystemStatus.errorCode == ERROR_NONE)
            SystemStatus.errorCode = ERROR_I2C_FAULT;
        System_SetState(STATE_ERROR);
    }
}

static void lcd_clear(void)
{
    lcd_send_cmd(0x01);
    HAL_Delay(2);
}

static void lcd_set_cursor(int row, int col)
{
    switch (row)
    {
        case 0: col |= 0x80; break;
        case 1: col |= 0xC0; break;
    }
    lcd_send_cmd(col);
}

static void lcd_send_string(char *str)
{
    while (*str)
    {
        lcd_send_data(*str);
        str++;
    }
}

/* =========================================================================
 * PRIVATE HELPER: pad buffer to exactly 16 chars and send to current cursor
 * ========================================================================= */
static void lcd_send_padded(char *buffer)
{
    int len = strlen(buffer);
    while (len < 16)
    {
        buffer[len] = ' ';
        len++;
    }
    buffer[16] = '\0';
    lcd_send_string(buffer);
}

/* =========================================================================
 * UI_Init — LCD hardware initialisation sequence (unchanged from original)
 * ========================================================================= */
void UI_Init(void)
{
    HAL_Delay(50);
    lcd_send_nibble(0x30);
    HAL_Delay(5);
    lcd_send_nibble(0x30);
    HAL_Delay(1);
    lcd_send_nibble(0x30);
    HAL_Delay(10);
    lcd_send_nibble(0x20);  // Switch to 4-bit mode
    HAL_Delay(10);

    lcd_send_cmd(0x28);     // 4-bit, 2 lines, 5×8 font
    HAL_Delay(1);
    lcd_send_cmd(0x08);     // Display off
    HAL_Delay(1);
    lcd_clear();
    lcd_send_cmd(0x06);     // Entry mode: cursor right, no shift
    HAL_Delay(1);
    lcd_send_cmd(0x0C);     // Display on, cursor off, blink off

    HAL_TIM_Encoder_Start(&htim3, TIM_CHANNEL_ALL);
    last_encoder_count = __HAL_TIM_GET_COUNTER(&htim3);
}

/* =========================================================================
 * UI_HandleButtonPress — called from EXTI interrupt (main.c)
 *
 * Uses System_SetState() for ALL transitions so stateChanged is always set.
 * ========================================================================= */
void UI_HandleButtonPress(void)
{
    /* Software debounce */
    unsigned int current_time = HAL_GetTick();
    if ((current_time - last_button_press_time) < 50)
        return;
    last_button_press_time = current_time;

    switch (SystemStatus.currentState)
    {
        case STATE_IDLE:
            /* Reset pill count and start the run */
            SystemStatus.currentPillCount = 0;
            SystemStatus.lastPillTime     = HAL_GetTick(); // Prevents instant inventory timeout
            System_SetState(STATE_RUNNING);
            break;

        case STATE_RUNNING:
            /* Manual emergency stop — always allowed */
            System_SetState(STATE_COMPLETE);
            break;

        case STATE_COMPLETE:
            /* Acknowledge completion and return to IDLE for next run */
            SystemStatus.currentPillCount = 0;
            System_SetState(STATE_IDLE);
            break;

        case STATE_ERROR:
            /* Acknowledge the fault, clear indicator LED, return to IDLE */
            BSP_LED_Off(LED_RED);
            BSP_LED_Off(LED_YELLOW);
            SystemStatus.errorCode = ERROR_NONE;
            System_SetState(STATE_IDLE);
            break;

        default:
            break;
    }
}

/* =========================================================================
 * UI_Update — called every main loop iteration
 *
 * 1. Reads rotary encoder in IDLE state and updates targetPillCount.
 * 2. Refreshes the LCD only when displayed values have changed.
 * 3. Rate-limits LCD writes to 100ms to prevent I2C traffic blocking
 *    the main loop during fast pill counting.
 * ========================================================================= */
void UI_Update(void)
{
    char buffer[17]; // 16 chars + null terminator

    /* ------------------------------------------------------------------
     * GUARD: If the LCD itself caused the I2C fault, stop retrying it.
     * Without this, every UI_Update call attempts I2C, fails, calls
     * System_SetState(STATE_ERROR) again, and any state transition the
     * button makes gets immediately overwritten back to STATE_ERROR.
     * The user must press the button to acknowledge and return to IDLE —
     * at which point we reset errorCode and retry will happen on next run.
     * ------------------------------------------------------------------ */
    if (SystemStatus.errorCode == ERROR_I2C_FAULT)
        return;

    /* ------------------------------------------------------------------
     * ENCODER READING — only active in STATE_IDLE
     * ------------------------------------------------------------------ */
    if (SystemStatus.currentState == STATE_IDLE)
    {
        uint16_t current_count = __HAL_TIM_GET_COUNTER(&htim3);
        int16_t  delta         = (int16_t)(current_count - last_encoder_count);

        if (delta >= 4 || delta <= -4)
        {
            int16_t clicks = delta / 4;

            if (clicks > 0)
            {
                SystemStatus.targetPillCount += (unsigned int)clicks;
                // Cap at a sensible maximum to prevent unsigned overflow
                if (SystemStatus.targetPillCount > 9999)
                    SystemStatus.targetPillCount = 9999;
            }
            else if (clicks < 0)
            {
                unsigned int decrement = (unsigned int)(-clicks);
                if (SystemStatus.targetPillCount > decrement)
                    SystemStatus.targetPillCount -= decrement;
                else
                    SystemStatus.targetPillCount = 0;
            }
            last_encoder_count += (int16_t)(clicks * 4);
        }
    }

    /* ------------------------------------------------------------------
     * SNAPSHOT — grab volatile fields once to prevent mid-update races
     * ------------------------------------------------------------------ */
    SystemState_t snap_state   = SystemStatus.currentState;
    unsigned int  snap_target  = SystemStatus.targetPillCount;
    unsigned int  snap_current = SystemStatus.currentPillCount;
    SystemError_t snap_error   = SystemStatus.errorCode;

    /* ------------------------------------------------------------------
     * LCD RATE LIMITING — only update at most every 100ms.
     * The pill count interrupt fires instantly regardless of this limit.
     * This decouples fast sensor events from slow I2C LCD updates.
     * State/error changes bypass the rate limit so DONE!/ERROR
     * messages appear immediately.
     * ------------------------------------------------------------------ */
    static unsigned int last_lcd_update = 0;
    unsigned int now = HAL_GetTick();

    uint8_t force_update = (snap_state != last_displayed_state ||
                            snap_error != last_displayed_error);

    if (!force_update && (now - last_lcd_update) < 100)
        return;

    /* ------------------------------------------------------------------
     * LCD REFRESH — only when something has actually changed
     * ------------------------------------------------------------------ */
    if (snap_state   == last_displayed_state   &&
        snap_target  == last_displayed_target  &&
        snap_current == last_displayed_current &&
        snap_error   == last_displayed_error)
    {
        return;
    }

    last_lcd_update = now;

    switch (snap_state)
    {
        /* ----------------------------------------------------------------
         * IDLE — show mode and current target
         * ---------------------------------------------------------------- */
        case STATE_IDLE:
            lcd_set_cursor(0, 0);
            if (snap_target == 0)
            {
                lcd_send_string("Inventory Check ");
                lcd_set_cursor(1, 0);
                snprintf(buffer, sizeof(buffer), "Target: NONE    ");
            }
            else
            {
                lcd_send_string("Ready to Count  ");
                lcd_set_cursor(1, 0);
                snprintf(buffer, sizeof(buffer), "Target: %u", snap_target);
            }
            lcd_send_padded(buffer);
            break;

        /* ----------------------------------------------------------------
         * RUNNING — live count display
         * ---------------------------------------------------------------- */
        case STATE_RUNNING:
            lcd_set_cursor(0, 0);
            lcd_send_string("Counting...     ");
            lcd_set_cursor(1, 0);
            if (snap_target == 0)
                snprintf(buffer, sizeof(buffer), "Total: %u", snap_current);
            else
                snprintf(buffer, sizeof(buffer), "%u / %u", snap_current, snap_target);
            lcd_send_padded(buffer);
            break;

        /* ----------------------------------------------------------------
         * COMPLETE — show final count
         * ---------------------------------------------------------------- */
        case STATE_COMPLETE:
            lcd_set_cursor(0, 0);
            lcd_send_string("DONE!           ");
            lcd_set_cursor(1, 0);
            snprintf(buffer, sizeof(buffer), "Pills: %u", snap_current);
            lcd_send_padded(buffer);
            break;

        /* ----------------------------------------------------------------
         * ERROR — line 1: fault header, line 2: specific error code.
         * LED is managed exclusively by SystemConfig.c entry actions —
         * no BSP_LED calls here to avoid conflicting with the FSM LED state.
         * ---------------------------------------------------------------- */
        case STATE_ERROR:
            lcd_set_cursor(0, 0);
            lcd_send_string("** FAULT **     ");
            lcd_set_cursor(1, 0);

            switch (snap_error)
            {
                case ERROR_JAM:
                    snprintf(buffer, sizeof(buffer), "Pill jam detect ");
                    break;
                case ERROR_NO_START:
                    snprintf(buffer, sizeof(buffer), "No pills at all ");
                    break;
                case ERROR_I2C_FAULT:
                    snprintf(buffer, sizeof(buffer), "I2C bus fault   ");
                    break;
                case ERROR_SENSOR_FAULT:
                    snprintf(buffer, sizeof(buffer), "Sensor error    ");
                    break;
                case ERROR_NONE:
                default:
                    snprintf(buffer, sizeof(buffer), "Unknown error   ");
                    break;
            }
            lcd_send_padded(buffer);

            /* NOTE: LED_YELLOW was removed here — LED state is managed
             * exclusively in SystemConfig.c to avoid conflicts. */
            break;

        default:
            buffer[0] = '\0';
            break;
    }

    /* Update trackers */
    last_displayed_state   = snap_state;
    last_displayed_target  = snap_target;
    last_displayed_current = snap_current;
    last_displayed_error   = snap_error;
}