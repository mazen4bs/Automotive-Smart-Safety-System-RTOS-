#include "gear_system.h"
#include "TM4C123GH6PM.h"
#include "speed_system.h"
#include <stdio.h>

// Global variables
static Gear_t currentGear = GEAR_DRIVE;  // Default to Drive

// Speed threshold for gear change
#define GEAR_CHANGE_SPEED_THRESHOLD 5.0f  // Increased threshold to 5 km/h for more stable gear changes

// Initialize GPIO for gear switches
void GearSystem_Init(void) {
    // Enable GPIO port for switches (using Port F)
    SYSCTL->RCGCGPIO |= (1 << 5);  // Enable GPIOF
    while((SYSCTL->PRGPIO & (1 << 5)) == 0); // Wait for GPIOF to be ready
    
    // Unlock PF0
    GPIOF->LOCK = 0x4C4F434B;
    GPIOF->CR |= (1 << 0);
    GPIOF->LOCK = 0;
    
    // Configure PF0 and PF1 as inputs with pull-up resistors
    GPIOF->DIR &= ~((1 << 0) | (1 << 1));  // Set as inputs
    GPIOF->PUR |= ((1 << 0) | (1 << 1));   // Enable pull-up resistors
    GPIOF->DEN |= ((1 << 0) | (1 << 1));   // Enable digital function
}

// Update gear reading
uint8_t GearSystem_Update(void) {
    uint32_t switchState;
    static Gear_t lastGear = GEAR_DRIVE;
    float currentSpeed = SpeedSystem_GetCurrentSpeed();
    
    // Read switch states
    switchState = GPIOF->DATA & ((1 << 0) | (1 << 1));
    
    // Only allow gear change if speed is below threshold
    if (currentSpeed <= GEAR_CHANGE_SPEED_THRESHOLD) {
        // Determine gear based on switch states
        if (switchState & (1 << 0) && !(switchState & (1 << 1))) {  // Drive switch pressed
            if (currentGear != GEAR_DRIVE) {
                currentGear = GEAR_DRIVE;
                return 1;  // Indicate gear changed
            }
        } else if (switchState & (1 << 1) && !(switchState & (1 << 0))) {  // Reverse switch pressed
            if (currentGear != GEAR_REVERSE) {
                currentGear = GEAR_REVERSE;
                return 1;  // Indicate gear changed
            }
        }	else {
					if(currentGear != GEAR_PARK) {
                currentGear = GEAR_PARK;
                return 1;  // Indicate gear changed
					}
				}
    }
    
    return 0;  // No change
}

// Get current gear
Gear_t GearSystem_GetCurrentGear(void) {
    return currentGear;
} 