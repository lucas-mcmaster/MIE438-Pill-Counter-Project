/*
 * SystemConfig.h
 *
 * Central FSM configuration header for the MIE438 Pill Counter.
 * Defines all states, error codes, and the global SystemStatus struct.
 */

#ifndef INC_SYSTEMCONFIG_H_
#define INC_SYSTEMCONFIG_H_

#include "stm32h7xx_hal.h"
#include "stm32h7xx_nucleo.h"

/* =========================================================================
 * STATE ENUMERATION
 * =========================================================================
 * Add new states here as needed. Each state maps to a distinct operating
 * mode of the pill counter machine.
 * ========================================================================= */
typedef enum {
    STATE_IDLE,         // Waiting for user to set target and press start
    STATE_RUNNING,      // Motor spinning, sensor actively counting pills
    STATE_COMPLETE,     // Target reached (or manually stopped), motor halted
    STATE_ERROR         // Fault detected — motor halted, user must acknowledge
} SystemState_t;

/* =========================================================================
 * ERROR CODE ENUMERATION
 * =========================================================================
 * Stored in SystemStatus.errorCode to allow UI and future modules to
 * distinguish the cause of STATE_ERROR without separate flag variables.
 * ========================================================================= */
typedef enum {
    ERROR_NONE          = 0x00,  // No error — normal operation
    ERROR_JAM           = 0x01,  // Motor running but pills stopped mid-count (jam detected)
    ERROR_SENSOR_FAULT  = 0x02,  // IR sensor triggered unexpectedly (bounce/debris)
    ERROR_I2C_FAULT     = 0x03,  // I2C communication failure (LCD)
    ERROR_NO_START      = 0x04   // Motor ran for JAM_COLD_START_MS with zero pill drops
} SystemError_t;

/* =========================================================================
 * JAM DETECTION TIMING CONSTANTS
 * =========================================================================
 * JAM_COLD_START_MS  : How long to wait for the FIRST pill before calling
 *                      it a jam/blockage. Motor has been running but nothing
 *                      has passed the sensor at all. (8 seconds)
 *
 * JAM_MID_COUNT_MS   : If counting was progressing normally but then stalls
 *                      for this duration, suspect a mid-count jam. Only
 *                      applies in target-count mode (targetPillCount > 0)
 *                      because inventory mode uses the 10s timeout to stop
 *                      naturally. (5 seconds)
 *
 * INVENTORY_TIMEOUT_MS : In inventory mode (target = 0), automatically
 *                        transition to COMPLETE after this idle time.
 * ========================================================================= */
#define JAM_COLD_START_MS       8000U
#define JAM_MID_COUNT_MS        5000U
#define INVENTORY_TIMEOUT_MS    10000U

/* =========================================================================
 * GLOBAL SYSTEM STATUS STRUCT
 * =========================================================================
 * Single source of truth shared (as extern) across all modules.
 * Keep fields grouped by function for readability.
 * ========================================================================= */
typedef struct {

    /* --- Core FSM -------------------------------------------------------- */
    SystemState_t currentState;     // Active FSM state
    uint8_t       stateChanged;     // Set to 1 on every state transition,
                                    // cleared after entry-actions execute.
                                    // Allows one-shot entry logic per state.

    /* --- Pill Count ------------------------------------------------------ */
    unsigned int targetPillCount;   // Desired count set via rotary encoder
                                    // (0 = inventory mode — count until stopped)
    unsigned int currentPillCount;  // Pills detected so far this run

    /* --- Timing ---------------------------------------------------------- */
    unsigned int filterDelayMs;     // Debounce window after each pill drop (ms)
    unsigned int lastPillTime;      // HAL_GetTick() timestamp of last pill drop
    unsigned int runStartTime;      // HAL_GetTick() when STATE_RUNNING was entered.
                                    // Used for cold-start jam detection.

    /* --- Error Reporting ------------------------------------------------- */
    SystemError_t errorCode;        // Populated before entering STATE_ERROR

} SystemConfig_t;

/* =========================================================================
 * EXTERN DECLARATION
 * =========================================================================
 * Volatile + extern so all translation units share the same instance and
 * the compiler never caches stale values from ISR-modified fields.
 * ========================================================================= */
extern volatile SystemConfig_t SystemStatus;

/* =========================================================================
 * PUBLIC FUNCTION PROTOTYPES
 * ========================================================================= */
void System_Init(void);
void System_ProcessState(void);

/* Internal helper — transitions state and sets the stateChanged flag.
 * Use this instead of direct assignment to guarantee entry-action firing. */
void System_SetState(SystemState_t newState);

#endif /* INC_SYSTEMCONFIG_H_ */
