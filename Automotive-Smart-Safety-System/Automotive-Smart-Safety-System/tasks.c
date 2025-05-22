#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "speed_system.h"
#include "gear_system.h"
#include "lcd.h"
#include "display.h"
#include "Door.h"
#include "TM4C123GH6PM.h"
#include <stdio.h>
#include "ultrasonic_system.h"

// Buzzer pin definitions (update to PE5)
#define BUZZER_PORT GPIOE
#define BUZZER_PIN 5

// Door Lock Task - Handles door locking/unlocking logic
void vDoorLockTask(void *pvParameters) {
    LCD_Message_t displayMsg;
    DoorState_t currentDoorState = DOORS_UNLOCKED;
    static DoorState_t lastDoorState = DOORS_UNLOCKED;
    static uint32_t statusDisplayTime = 0;
    static uint8_t showingStatus = 0;
    
    while(1) {
        // Check for door state changes
        if(DoorSystem_Update()) {  // Only update if state changed
            uint32_t currentTime = xTaskGetTickCount();
            
            // Get current state
            currentDoorState = DoorSystem_GetState();
            
            xSemaphoreTake(xLCDMutex, portMAX_DELAY);
            LCD_set_cursor(0, 0);
            
            // Check if lock state changed
            if (currentDoorState != lastDoorState) {
                if (currentDoorState == DOORS_LOCKED) {
                    snprintf(displayMsg.line1, sizeof(displayMsg.line1), "Door: Locked  ");
                } else {
                    snprintf(displayMsg.line1, sizeof(displayMsg.line1), "Door: Unlocked");
                }
                showingStatus = 1;
                statusDisplayTime = currentTime;
            }
            // If showing status message and 2 seconds have passed, return to normal display
            else if (showingStatus && (currentTime - statusDisplayTime > pdMS_TO_TICKS(2000))) {
                snprintf(displayMsg.line1, sizeof(displayMsg.line1), "Door: %s", 
                        (currentDoorState == DOORS_UNLOCKED) ? "Unlocked" : "Locked  ");
                showingStatus = 0;
            }
            
            LCD_write_string(displayMsg.line1);
            xSemaphoreGive(xLCDMutex);
            
            // Update last state
            lastDoorState = currentDoorState;
        }
        
        // Small delay for debouncing and task switching
        vTaskDelay(pdMS_TO_TICKS(100)); // 100ms delay
    }
}

// Door Open/Close Task - Handles door open/closed state
void vDoorOpenCloseTask(void *pvParameters) {
    LCD_Message_t displayMsg;
    DoorOpenState_t currentDoorOpenState = DOOR_CLOSED;
    static DoorOpenState_t lastDoorOpenState = DOOR_CLOSED;
    static uint32_t statusDisplayTime = 0;
    static uint8_t showingStatus = 0;
    
    // Enable GPIOE for buzzer
    SYSCTL->RCGCGPIO |= (1 << 4); // Enable clock for Port E
    while((SYSCTL->PRGPIO & (1 << 4)) == 0); // Wait for Port E ready
    // Configure PE1 as output for buzzer
    GPIOE->DIR |= (1 << 1);
    GPIOE->DEN |= (1 << 1);
    GPIOE->DATA &= ~(1 << 1); // Buzzer off initially
    
    while(1) {
        // Get current door open state and speed
        currentDoorOpenState = DoorSystem_GetOpenState();
        float currentSpeed = SpeedSystem_GetCurrentSpeed();
        
        // Check if door open state changed
        if (currentDoorOpenState != lastDoorOpenState) {
            uint32_t currentTime = xTaskGetTickCount();
            
            xSemaphoreTake(xLCDMutex, portMAX_DELAY);
            LCD_set_cursor(0, 0);
            
            if (currentDoorOpenState == DOOR_OPEN) {
                snprintf(displayMsg.line1, sizeof(displayMsg.line1), "Door: Opened  ");
            } else {
                snprintf(displayMsg.line1, sizeof(displayMsg.line1), "Door: Closed  ");
            }
            
            LCD_write_string(displayMsg.line1);
            xSemaphoreGive(xLCDMutex);
            
            showingStatus = 1;
            statusDisplayTime = currentTime;
            
            // Update last state
            lastDoorOpenState = currentDoorOpenState;
        }
        // If showing status message and 2 seconds have passed, return to normal display
        else if (showingStatus && (xTaskGetTickCount() - statusDisplayTime > pdMS_TO_TICKS(2000))) {
            xSemaphoreTake(xLCDMutex, portMAX_DELAY);
            LCD_set_cursor(0, 0);
            snprintf(displayMsg.line1, sizeof(displayMsg.line1), "Door: %s", 
                    (DoorSystem_GetState() == DOORS_UNLOCKED) ? "Unlocked" : "Locked  ");
            LCD_write_string(displayMsg.line1);
            xSemaphoreGive(xLCDMutex);
            
            showingStatus = 0;
        }
        
        // Handle buzzer for door open while moving
        if (currentDoorOpenState == DOOR_OPEN && currentSpeed > 0.0f) {
            // Continuous buzzer for door open warning
            GPIOE->DATA |= (1 << 1); // Buzzer ON continuously
            
            xSemaphoreTake(xLCDMutex, portMAX_DELAY);
            LCD_set_cursor(1, 0);
            snprintf(displayMsg.line2, sizeof(displayMsg.line2), "WARNING: Door Open! ");
            LCD_write_string(displayMsg.line2);
            xSemaphoreGive(xLCDMutex);
        } else {
            GPIOE->DATA &= ~(1 << 1); // Buzzer OFF
            // Restore speed display if not in reverse
            if (GearSystem_GetCurrentGear() != GEAR_REVERSE) {
                xSemaphoreTake(xLCDMutex, portMAX_DELAY);
                LCD_set_cursor(1, 0);
                snprintf(displayMsg.line2, sizeof(displayMsg.line2), "Speed=%.1f km/h  ", currentSpeed);
                LCD_write_string(displayMsg.line2);
                xSemaphoreGive(xLCDMutex);
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(100)); // 100ms delay
    }
}

// Ultrasonic Task - Handles distance measurement and display
void vUltrasonicTask(void *pvParameters) {
    LCD_Message_t displayMsg;
    float distance = 0.0f;
    static uint32_t lastBeepTime = 0;
    static uint8_t isInReverse = 0;
    static Gear_t lastGear = GEAR_PARK;
    
    // Initialize ultrasonic system
    UltrasonicSystem_Init();
    
    // Turn off all indicators at start
    UltrasonicSystem_TurnOffLEDs();
    UltrasonicSystem_TurnOffBuzzer();
    
    while(1) {
        Gear_t currentGear = GearSystem_GetCurrentGear();
        
        // Check for gear change
        if (currentGear != lastGear) {
            // Reset states on gear change
            isInReverse = 0;
            lastBeepTime = 0;
            UltrasonicSystem_TurnOffLEDs();
            UltrasonicSystem_TurnOffBuzzer();
        }
        
        if (currentGear == GEAR_REVERSE) {
            // Update and get distance measurement
            UltrasonicSystem_Update();  // This updates the distance measurement
            distance = UltrasonicSystem_GetDistance();
            
            // Update display based on distance
            xSemaphoreTake(xLCDMutex, portMAX_DELAY);
            LCD_set_cursor(1, 0);
            if (distance < 150.0f && distance > 0.0f) {  // Only show distance if it's valid
                // Show distance if less than max
                snprintf(displayMsg.line2, sizeof(displayMsg.line2), "Dist=%.1f cm  ", distance);
                // Update LEDs and buzzer only when distance is less than 150cm
                UltrasonicSystem_UpdateLEDs(distance);
                UltrasonicSystem_UpdateBuzzer(distance);
            } else {
                // Show speed if at max distance or invalid reading
                float currentSpeed = SpeedSystem_GetCurrentSpeed();
                snprintf(displayMsg.line2, sizeof(displayMsg.line2), "Speed=%.1f km/h  ", currentSpeed);
                // Turn off LEDs and buzzer when at max distance
                UltrasonicSystem_TurnOffLEDs();
                UltrasonicSystem_TurnOffBuzzer();
            }
            LCD_write_string(displayMsg.line2);
            xSemaphoreGive(xLCDMutex);
            
            isInReverse = 1;
        } else {
            // Not in reverse, turn off all indicators
            UltrasonicSystem_TurnOffLEDs();
            UltrasonicSystem_TurnOffBuzzer();
            isInReverse = 0;
        }
        
        lastGear = currentGear;
        vTaskDelay(pdMS_TO_TICKS(100)); // 100ms delay
    }
}

// Speed Task - Monitors vehicle speed and controls auto-lock
void vSpeedTask(void *pvParameters) {
    float currentSpeed = 0.0f;
    static float lastSpeed = 0.0f;
    
    while(1) {
        // Update speed
        SpeedSystem_Update();
        currentSpeed = SpeedSystem_GetCurrentSpeed();
        
        // Check if speed has dropped below threshold after being above it
        if (lastSpeed > 20.0f && currentSpeed <= 20.0f) {
            DoorSystem_ResetManualOverride();  // Reset manual override when speed drops below threshold
        }
        
        lastSpeed = currentSpeed;
        vTaskDelay(pdMS_TO_TICKS(100)); // Update every 100ms
    }
}

// Gear Task - Monitors gear changes and updates display
void vGearTask(void *pvParameters) {
    LCD_Message_t displayMsg;
    Gear_t lastGear = GEAR_DRIVE;
    
    while(1) {
        // Check for gear changes
        if(GearSystem_Update()) {  // Only update if gear changed
            xSemaphoreTake(xLCDMutex, portMAX_DELAY);
            LCD_set_cursor(0, 15);
            const char* gearText;
            switch(GearSystem_GetCurrentGear()) {
                case GEAR_PARK:
                    gearText = "P";
                    break;
                case GEAR_DRIVE:
                    gearText = "D";
                    break;
                case GEAR_REVERSE:
                    gearText = "R";
                    break;
                default:
                    gearText = "Unknown";
            }
            snprintf(displayMsg.line1, sizeof(displayMsg.line1), gearText);
            LCD_write_string(displayMsg.line1);
            xSemaphoreGive(xLCDMutex);
        }
        
        vTaskDelay(pdMS_TO_TICKS(50)); // 50ms delay
    }
}

// Display Task - Updates LCD with system status
void vDisplayTask(void *pvParameters) {
    LCD_Message_t displayMsg;
    
    // Initial display setup
    xSemaphoreTake(xLCDMutex, portMAX_DELAY);
    LCD_Clear();
    LCD_set_cursor(0, 0);
    snprintf(displayMsg.line1, sizeof(displayMsg.line1), "Door: %s", 
             (DoorSystem_GetState() == DOORS_UNLOCKED) ? "Unlocked" : "Locked  ");
    LCD_write_string(displayMsg.line1);
    
    LCD_set_cursor(0, 15);
    const char* gearText;
    switch(GearSystem_GetCurrentGear()) {
        case GEAR_PARK:
            gearText = "P";
            break;
        case GEAR_DRIVE:
            gearText = "D";
            break;
        case GEAR_REVERSE:
            gearText = "R";
            break;
        default:
            gearText = "Unknown";
    }
    snprintf(displayMsg.line1, sizeof(displayMsg.line1), gearText);
    LCD_write_string(displayMsg.line1);
    
    LCD_set_cursor(1, 0);
    if(GearSystem_GetCurrentGear() == GEAR_REVERSE) {
        snprintf(displayMsg.line2, sizeof(displayMsg.line2), "Dist=%.1f cm  ", 
                 UltrasonicSystem_GetDistance());
    } else {
        snprintf(displayMsg.line2, sizeof(displayMsg.line2), "Speed=%.1f km/h  ", 
                 SpeedSystem_GetCurrentSpeed());
    }
    LCD_write_string(displayMsg.line2);
    xSemaphoreGive(xLCDMutex);
    
    while(1) {
        // Only update gear display, let speed task handle speed/distance
        xSemaphoreTake(xLCDMutex, portMAX_DELAY);
        LCD_set_cursor(0, 15);
        const char* gearText;
        switch(GearSystem_GetCurrentGear()) {
            case GEAR_PARK:
                gearText = "P";
                break;
            case GEAR_DRIVE:
                gearText = "D";
                break;
            case GEAR_REVERSE:
                gearText = "R";
                break;
            default:
                gearText = "Unknown";
        }
        snprintf(displayMsg.line1, sizeof(displayMsg.line1), gearText);
        LCD_write_string(displayMsg.line1);
        xSemaphoreGive(xLCDMutex);
        
        vTaskDelay(pdMS_TO_TICKS(1000)); // Update every second
    }
}

// Ignition Status Task - Displays ignition status on LCD
void vIgnitionStatusTask(void *pvParameters) {
    LCD_Message_t displayMsg;
    static uint8_t lastIgnitionState = 1;  // Default high (ignition on)
    static uint32_t statusDisplayTime = 0;
    static uint8_t showingStatus = 0;
    
    while(1) {
        uint8_t currentIgnitionState = DoorSystem_IsIgnitionOn();
        
        // Check if ignition state changed
        if (currentIgnitionState != lastIgnitionState) {
            uint32_t currentTime = xTaskGetTickCount();
            
            xSemaphoreTake(xLCDMutex, portMAX_DELAY);
            // Only update the first line, preserve the second line
            LCD_set_cursor(0, 0);
            
            if (currentIgnitionState) {
                snprintf(displayMsg.line1, sizeof(displayMsg.line1), "Ignition: ON  ");
            } else {
                snprintf(displayMsg.line1, sizeof(displayMsg.line1), "Ignition: OFF ");
            }
            
            LCD_write_string(displayMsg.line1);
            xSemaphoreGive(xLCDMutex);
            
            showingStatus = 1;
            statusDisplayTime = currentTime;
            
            // Update last state
            lastIgnitionState = currentIgnitionState;
        }
        // If showing status message and 1 second has passed, return to normal display
        else if (showingStatus && (xTaskGetTickCount() - statusDisplayTime > pdMS_TO_TICKS(1000))) {
            xSemaphoreTake(xLCDMutex, portMAX_DELAY);
            LCD_set_cursor(0, 0);
            snprintf(displayMsg.line1, sizeof(displayMsg.line1), "Door: %s", 
                    (DoorSystem_GetState() == DOORS_UNLOCKED) ? "Unlocked" : "Locked  ");
            LCD_write_string(displayMsg.line1);
            xSemaphoreGive(xLCDMutex);
            
            showingStatus = 0;
        }
        
        vTaskDelay(pdMS_TO_TICKS(100)); // 100ms delay
    }
}
