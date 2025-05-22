#include "Door.h"
#include <string.h>
#include "TM4C123GH6PM.h"
#include "speed_system.h"
#include "gear_system.h"

// Define GPIO pins for lock/unlock buttons
#define LOCK_BTN_PORT      GPIOB
#define LOCK_BTN_PIN       (1 << 0)  // PB0 - External button for LOCK
#define UNLOCK_BTN_PORT    GPIOF
#define UNLOCK_BTN_PIN     (1 << 4)  // PF4 - Onboard SW1 for UNLOCK
#define IGNITION_PORT      GPIOF
#define IGNITION_PIN       (1 << 3)  // PF3 - Ignition switch
#define DOOR_SWITCH_PORT   GPIOF
#define DOOR_SWITCH_PIN    (1 << 2)  // PF2 - Door open/closed switch

// Speed threshold for auto-lock (in km/h)
#define AUTO_LOCK_SPEED_THRESHOLD 20.0f

// Direct register definitions if needed
#define GPIO_PORTF_LOCK_R  (*((volatile uint32_t *)0x40025520))
#define GPIO_PORTF_CR_R    (*((volatile uint32_t *)0x40025524))

// Door state
static DoorState_t currentDoorState = DOORS_UNLOCKED;
static DoorOpenState_t currentDoorOpenState = DOOR_CLOSED;
static uint8_t manualOverride = 0;  // Flag to track if manual override is active
static uint8_t lastIgnitionState = 1;  // Default high (ignition on)
static uint8_t ignitionState = 1;      // Current ignition state

// Function to initialize door control system
void DoorSystem_Init(void) {
    // Enable Port B for external button
    SYSCTL->RCGCGPIO |= (1 << 1); // Enable clock for GPIOB
    
    // Enable Port F for onboard switch and ignition
    SYSCTL->RCGCGPIO |= (1 << 5); // Enable clock for GPIOF
    
    // Wait for clocks to stabilize
    while((SYSCTL->PRGPIO & ((1 << 1) | (1 << 5))) != ((1 << 1) | (1 << 5)));
    
    // Configure PB0 (external LOCK button) as input with pull-up
    LOCK_BTN_PORT->DIR &= ~LOCK_BTN_PIN;    // Set as input
    LOCK_BTN_PORT->PUR |= LOCK_BTN_PIN;     // Enable pull-up
    LOCK_BTN_PORT->DEN |= LOCK_BTN_PIN;     // Digital enable
    
    // Configure PF4 (onboard UNLOCK button) as input with pull-up
    UNLOCK_BTN_PORT->DIR &= ~UNLOCK_BTN_PIN;  // Set as input
    UNLOCK_BTN_PORT->PUR |= UNLOCK_BTN_PIN;   // Enable pull-up
    UNLOCK_BTN_PORT->DEN |= UNLOCK_BTN_PIN;   // Digital enable
    
    // Configure PF3 (ignition switch) as input with pull-up
    IGNITION_PORT->DIR &= ~IGNITION_PIN;     // Set as input
    IGNITION_PORT->PUR |= IGNITION_PIN;      // Enable pull-up
    IGNITION_PORT->DEN |= IGNITION_PIN;      // Digital enable

    // Configure PF2 (door switch) as input with pull-up
    DOOR_SWITCH_PORT->DIR &= ~DOOR_SWITCH_PIN;  // Set as input
    DOOR_SWITCH_PORT->PUR |= DOOR_SWITCH_PIN;   // Enable pull-up
    DOOR_SWITCH_PORT->DEN |= DOOR_SWITCH_PIN;   // Digital enable
}

// Function to check if ignition is on
uint8_t DoorSystem_IsIgnitionOn(void) {
    return ignitionState;
}

// Function to get current door open state
DoorOpenState_t DoorSystem_GetOpenState(void) {
    return currentDoorOpenState;
}

// Function to set door open state
void DoorSystem_SetOpenState(DoorOpenState_t state) {
    currentDoorOpenState = state;
}

// Function to check buttons and update door state
uint8_t DoorSystem_Update(void) {
    static uint8_t prevLockState = 1;    // Default high with pull-up
    static uint8_t prevUnlockState = 1;  // Default high with pull-up
    static uint8_t prevDoorSwitchState = 1;  // Default high with pull-up
    static uint32_t lastDebounceTime = 0;
    static uint8_t debounceDelay = 50;   // 50ms debounce delay
    
    // Read current button states (0 = pressed, 1 = not pressed due to pull-ups)
    uint8_t lockBtnState = (LOCK_BTN_PORT->DATA & LOCK_BTN_PIN) ? 1 : 0;
    uint8_t unlockBtnState = (UNLOCK_BTN_PORT->DATA & UNLOCK_BTN_PIN) ? 1 : 0;
    uint8_t newIgnitionState = (IGNITION_PORT->DATA & IGNITION_PIN) ? 1 : 0;
    uint8_t doorSwitchState = (DOOR_SWITCH_PORT->DATA & DOOR_SWITCH_PIN) ? 1 : 0;
    
    // Get current time
    uint32_t currentTime = xTaskGetTickCount();
    
    // Check for door switch state change with debounce
    if (doorSwitchState != prevDoorSwitchState) {
        if ((currentTime - lastDebounceTime) > debounceDelay) {
            // Update door open state (0 = door open, 1 = door closed due to pull-up)
            DoorSystem_SetOpenState(doorSwitchState ? DOOR_CLOSED : DOOR_OPEN);
            lastDebounceTime = currentTime;
        }
    }
    
    // Check for ignition state change
    if (newIgnitionState != ignitionState) {
        if (newIgnitionState == 0) {  // Attempting to turn ignition off
            float currentSpeed = SpeedSystem_GetCurrentSpeed();
            if (currentSpeed > 0.0f) {
                // Ignore ignition off if speed is not 0
                return 0;
            }
        } else {  // Attempting to turn ignition on
            // Only allow ignition on if in PARK gear
            if (GearSystem_GetCurrentGear() != GEAR_PARK) {
                // Force ignition to stay off
                newIgnitionState = 0;
            }
        }
        
        // Update ignition state
        ignitionState = newIgnitionState;
        
        // If ignition is turned off, unlock doors
        if (ignitionState == 0) {
            if (currentDoorState != DOORS_UNLOCKED) {
                DoorSystem_SetState(DOORS_UNLOCKED);
                manualOverride = 0;  // Reset manual override
                return 1;
            }
        }
    }
    
    // Check for lock button press (falling edge) with debounce
    if (lockBtnState == 0 && prevLockState == 1) {
        if ((currentTime - lastDebounceTime) > debounceDelay) {
            if (currentDoorState != DOORS_LOCKED) {
                DoorSystem_SetState(DOORS_LOCKED);
                manualOverride = 1;  // Set manual override flag
                lastDebounceTime = currentTime;
                return 1;
            }
        }
    }
    
    // Check for unlock button press (falling edge) with debounce
    if (unlockBtnState == 0 && prevUnlockState == 1) {
        if ((currentTime - lastDebounceTime) > debounceDelay) {
            if (currentDoorState != DOORS_UNLOCKED) {
                DoorSystem_SetState(DOORS_UNLOCKED);
                // Only set manual override if speed is not 0
                float currentSpeed = SpeedSystem_GetCurrentSpeed();
                if (currentSpeed > 0.0f) {
                    manualOverride = 1;
                } else {
                    manualOverride = 0;  // Reset manual override if speed is 0
                }
                lastDebounceTime = currentTime;
                return 1;
            }
        }
    }
    
    // Check for speed-based auto-lock if no manual override and ignition is on
    if (!manualOverride && ignitionState) {
        float currentSpeed = SpeedSystem_GetCurrentSpeed();
        if (currentSpeed > AUTO_LOCK_SPEED_THRESHOLD && currentDoorState != DOORS_LOCKED) {
            DoorSystem_SetState(DOORS_LOCKED);
            return 1;
        }
    }
    
    // Update previous states
    prevLockState = lockBtnState;
    prevUnlockState = unlockBtnState;
    prevDoorSwitchState = doorSwitchState;
    
    return 0;  // No change
}

// Function to get current door state
DoorState_t DoorSystem_GetState(void) {
    return currentDoorState;
}

// Function to set door state and update display
void DoorSystem_SetState(DoorState_t state) {
    currentDoorState = state;
}

// Function to reset manual override flag
void DoorSystem_ResetManualOverride(void) {
    manualOverride = 0;
}