/*
 * SystemConfig.c
 *
 * Full Finite State Machine (FSM) for the MIE438 Pill Counter.
 *
 * ARCHITECTURE OVERVIEW
 * ─────────────────────
 * The FSM runs inside the main while(1) loop via System_ProcessState().
 * All state transitions use System_SetState() which sets the stateChanged
 * flag so that one-shot "entry actions" (LED changes, resets, etc.) execute
 * exactly once per transition — not on every loop iteration.
 *
 * Interrupts (IR sensor, encoder button) are handled in main.c's
 * HAL_GPIO_EXTI_Callback and call into BeamSensorHandler / UserInterface.
 * Those modules write into SystemStatus fields; this file reads them.
 *
 * STATE DIAGRAM
 * ─────────────
 *
 *                     ┌─────────────────────────────┐
 *          Power On   │                             │  Button (from ERROR)
 *         ──────────► │         STATE_IDLE           │◄──────────────────────┐
 *                     │  • Motor off                 │                       │
 *                     │  • Encoder sets target       │                       │
 *                     │  • Green LED blink           │                       │
 *                     └────────────┬────────────────┘                       │
 *                                  │ Button press                            │
 *                                  ▼                                         │
 *                     ┌─────────────────────────────┐                       │
 *                     │        STATE_RUNNING          │                       │
 *                     │  • Motor spinning             │──── Jam detected ────►│
 *                     │  • Sensor counting            │    I2C fault          │
 *                     │  • Green LED solid            │                  STATE_ERROR
 *                     │  • Jam detection active       │  • Motor off          │
 *                     └────────────┬────────────────┘  • Red LED on          │
 *                                  │                    • LCD shows fault     │
 *                  Target reached  │  Manual stop                            │
 *                  OR inventory    │  OR timeout                             │
 *                  timeout         ▼                                         │
 *                     ┌─────────────────────────────┐                       │
 *                     │        STATE_COMPLETE         │                       │
 *                     │  • Motor off                  │                       │
 *                     │  • Yellow LED on              │                       │
 *                     │  • LCD shows final count      │                       │
 *                     └────────────┬────────────────┘                       │
 *                                  │ Button press                            │
 *                                  └────────────────────────────────────────►│
 *                                                              Back to IDLE  │
 *
 * NOTE: STATE_ERROR is reachable from STATE_RUNNING only (hardware faults
 * are only relevant when the machine is active). Button press in STATE_ERROR
 * returns to STATE_IDLE after clearing the fault.
 */

#include "SystemConfig.h"
#include "main.h"

/* =========================================================================
 * GLOBAL INSTANCE
 * ========================================================================= */
volatile SystemConfig_t SystemStatus;

/* =========================================================================
 * INTERNAL STATE TRANSITION HELPER
 * =========================================================================
 * Always use this instead of directly assigning SystemStatus.currentState.
 * Sets the stateChanged flag so entry-actions in System_ProcessState()
 * fire exactly once on the first loop after a transition.
 * ========================================================================= */
void System_SetState(SystemState_t newState)
{
    SystemStatus.currentState = newState;
    SystemStatus.stateChanged  = 1;
}

/* =========================================================================
 * SYSTEM INITIALIZATION — called once at power-up (main.c)
 * ========================================================================= */
void System_Init(void)
{
    /* FSM */
    SystemStatus.currentState    = STATE_IDLE;
    SystemStatus.stateChanged    = 1;  // Fire IDLE entry actions on first loop

    /* Counts */
    SystemStatus.targetPillCount  = 1;  // Default to 1 so user sees a sensible start
    SystemStatus.currentPillCount = 0;

    /* Timing */
    SystemStatus.filterDelayMs   = 20;  // IR sensor debounce — tune after hardware testing
    SystemStatus.lastPillTime    = 0;
    SystemStatus.runStartTime    = 0;

    /* Error */
    SystemStatus.errorCode       = ERROR_NONE;
}

/* =========================================================================
 * MAIN FSM — called every iteration of the main while(1) loop
 * =========================================================================
 * Structure per state:
 *   1. Entry actions  — runs ONCE on the first loop after a transition
 *                       (guarded by stateChanged flag).
 *   2. Ongoing logic  — runs EVERY loop while in this state.
 *   3. Transition checks — evaluate exit conditions and call System_SetState().
 * ========================================================================= */
void System_ProcessState(void)
{
    /* Snapshot tick once per call for consistent timing comparisons */
    unsigned int now = HAL_GetTick();

    switch (SystemStatus.currentState)
    {
        /* =================================================================
         * STATE_IDLE
         * Motor is off. User adjusts targetPillCount with the rotary encoder.
         * Pressing the encoder button (handled in UI_HandleButtonPress)
         * transitions to STATE_RUNNING.
         * UI_Update() handles encoder reading and LCD display in this state.
         * ================================================================= */
        case STATE_IDLE:

            /* ---- Entry actions (run once) -------------------------------- */
            if (SystemStatus.stateChanged)
            {
                SystemStatus.stateChanged = 0;

                /* Turn off all indicator LEDs, then set IDLE pattern */
                BSP_LED_Off(LED_GREEN);
                BSP_LED_Off(LED_YELLOW);
                BSP_LED_Off(LED_RED);

                /* Green LED blinks to indicate ready/idle.
                 * Full blink logic would go in a timer ISR or a non-blocking
                 * ticker; for now toggling it once on entry gives a visual cue.
                 * Extend with a dedicated blink counter if needed. */
                BSP_LED_On(LED_GREEN);

                /* Clear any leftover error from a previous run */
                SystemStatus.errorCode = ERROR_NONE;
            }

            /* ---- Ongoing logic ------------------------------------------ */
            /* Encoder reading and LCD update are handled by UI_Update() in
             * main.c. No additional logic needed here for IDLE. */

            /* ---- Transition check --------------------------------------- */
            /* STATE_IDLE → STATE_RUNNING is triggered by button press inside
             * UI_HandleButtonPress(). No polling needed here. */
            break;


        /* =================================================================
         * STATE_RUNNING
         * Motor is spinning (Motor_Update() handles step timing).
         * IR sensor interrupts call Sensor_HandlePillDrop() which increments
         * currentPillCount and updates lastPillTime.
         * This state monitors for completion and for faults.
         * ================================================================= */
        case STATE_RUNNING:

            /* ---- Entry actions (run once) -------------------------------- */
            if (SystemStatus.stateChanged)
            {
                SystemStatus.stateChanged = 0;

                /* Record when the run started for cold-start jam detection */
                SystemStatus.runStartTime  = now;

                /* Reset lastPillTime to now so the jam timer starts fresh.
                 * (UI_HandleButtonPress already does this but belt-and-suspenders) */
                SystemStatus.lastPillTime  = now;

                /* Solid green LED = machine is actively running */
                BSP_LED_Off(LED_YELLOW);
                BSP_LED_Off(LED_RED);
                BSP_LED_On(LED_GREEN);
            }

            /* ---- Ongoing logic: completion checks ----------------------- */

            /* TARGET COUNT MODE (targetPillCount > 0):
             * Transition to COMPLETE once the pill count meets or exceeds
             * the target. Using >= in case of a fast-falling double-detection
             * that slips past the debounce filter. */
            if (SystemStatus.targetPillCount > 0 &&
                SystemStatus.currentPillCount >= SystemStatus.targetPillCount)
            {
                System_SetState(STATE_COMPLETE);
                break;  // Exit switch immediately — entry actions run next loop
            }

            /* INVENTORY MODE (targetPillCount == 0):
             * No fixed target. Automatically stop after INVENTORY_TIMEOUT_MS
             * of inactivity (no pill drops). The user can also press the
             * button at any time to stop (handled in UI_HandleButtonPress). */
            if (SystemStatus.targetPillCount == 0 &&
                (now - SystemStatus.lastPillTime) >= INVENTORY_TIMEOUT_MS)
            {
                System_SetState(STATE_COMPLETE);
                break;
            }

            /* ---- Ongoing logic: jam detection --------------------------- */

            /* JAM TYPE 1 — Cold start: motor has been running for
             * JAM_COLD_START_MS but not a single pill has been detected yet.
             * Only checked in target-count mode because inventory mode is
             * expected to start slowly or have gaps between pills.
             *
             * Condition: currentPillCount == 0 means zero drops since start. */
            if (SystemStatus.targetPillCount > 0 &&
                SystemStatus.currentPillCount == 0 &&
                (now - SystemStatus.runStartTime) >= JAM_COLD_START_MS)
            {
                SystemStatus.errorCode = ERROR_NO_START;
                System_SetState(STATE_ERROR);
                break;
            }

            /* JAM TYPE 2 — Mid-count stall: counting was progressing but
             * has now stalled. Only fire if at least one pill was already
             * counted (otherwise it's covered by the cold-start check above).
             *
             * Only in target-count mode — inventory timeout handles this
             * scenario gracefully in inventory mode without raising an error. */
            if (SystemStatus.targetPillCount > 0 &&
                SystemStatus.currentPillCount > 0 &&
                SystemStatus.currentPillCount < SystemStatus.targetPillCount &&
                (now - SystemStatus.lastPillTime) >= JAM_MID_COUNT_MS)
            {
                SystemStatus.errorCode = ERROR_JAM;
                System_SetState(STATE_ERROR);
                break;
            }

            /* ---- No transitions fired — motor and sensor continue ------- */
            break;


        /* =================================================================
         * STATE_COMPLETE
         * Target reached, inventory timeout elapsed, or user manually stopped.
         * Motor_Update() already stops the motor when state != STATE_RUNNING.
         * Wait for the user to press the encoder button to return to IDLE.
         * ================================================================= */
        case STATE_COMPLETE:

            /* ---- Entry actions (run once) -------------------------------- */
            if (SystemStatus.stateChanged)
            {
                SystemStatus.stateChanged = 0;

                /* Yellow LED = job done, waiting for acknowledgement */
                BSP_LED_Off(LED_GREEN);
                BSP_LED_Off(LED_RED);
                BSP_LED_On(LED_YELLOW);

                /* Motor_Update() will call Motor_Stop() because state is no
                 * longer STATE_RUNNING. No explicit stop call needed here,
                 * but it can be added as a safety measure if desired:
                 *   Motor_Stop();
                 */
            }

            /* ---- Transition check --------------------------------------- */
            /* STATE_COMPLETE → STATE_IDLE is driven by button press in
             * UI_HandleButtonPress() which resets currentPillCount and calls
             * System_SetState(STATE_IDLE). Nothing to do here. */
            break;


        /* =================================================================
         * STATE_ERROR
         * A hardware or logic fault was detected while running.
         * Motor is stopped (Motor_Update exits early when not STATE_RUNNING).
         * Red LED is on. LCD shows the fault (UI_Update handles display).
         * User must press the encoder button to acknowledge and return to IDLE.
         *
         * errorCode is set BEFORE entering this state so the UI can display
         * a specific message for each fault type.
         * ================================================================= */
        case STATE_ERROR:

            /* ---- Entry actions (run once) -------------------------------- */
            if (SystemStatus.stateChanged)
            {
                SystemStatus.stateChanged = 0;

                /* Red LED on, all others off */
                BSP_LED_Off(LED_GREEN);
                BSP_LED_Off(LED_YELLOW);
                BSP_LED_On(LED_RED);

                /* Motor_Update() will stop the motor automatically since
                 * state is not STATE_RUNNING. */
            }

            /* ---- Transition check --------------------------------------- */
            /* STATE_ERROR → STATE_IDLE is driven by button press in
             * UI_HandleButtonPress() which clears the LED and calls
             * System_SetState(STATE_IDLE). Nothing to do here. */
            break;


        /* =================================================================
         * DEFAULT — should never be reached with a valid SystemState_t.
         * Treat as a critical fault to catch bad state corruption.
         * ================================================================= */
        default:
            SystemStatus.errorCode = ERROR_SENSOR_FAULT; // Reuse as generic fault
            System_SetState(STATE_ERROR);
            break;
    }
}
