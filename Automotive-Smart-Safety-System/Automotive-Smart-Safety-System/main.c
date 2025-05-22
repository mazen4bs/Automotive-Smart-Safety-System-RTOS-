#include "lcd.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "speed_system.h"
#include "gear_system.h"
#include "display.h"
#include "Door.h"
#include "TM4C123GH6PM.h"
#include "ultrasonic_system.h"

// Global handles
SemaphoreHandle_t xLCDMutex;

void vDoorLockTask(void *pvParameters);
void vDoorOpenCloseTask(void *pvParameters);
void vSpeedTask(void *pvParameters);
void vDisplayTask(void *pvParameters);
void vGearTask(void *pvParameters);
void vUltrasonicTask(void *pvParameters);
void vIgnitionStatusTask(void *pvParameters);

// Task handles
TaskHandle_t xDoorLockTaskHandle = NULL;
TaskHandle_t xDoorOpenCloseTaskHandle = NULL;
TaskHandle_t xSpeedTaskHandle = NULL;
TaskHandle_t xGearTaskHandle = NULL;
TaskHandle_t xDisplayTaskHandle = NULL;
TaskHandle_t xUltrasonicTaskHandle = NULL;
TaskHandle_t xIgnitionStatusTaskHandle = NULL;

int main(void) {
    // Initialize all systems
    SystemInit();
    LCD_Init();
    DoorSystem_Init();
    SpeedSystem_Init();
    GearSystem_Init();
    UltrasonicSystem_Init();  // Initialize ultrasonic system
    
    // Create mutex for LCD access
    xLCDMutex = xSemaphoreCreateMutex();
    
    // Create tasks
    xTaskCreate(vDoorLockTask, "DoorLock", 128, NULL, 3, NULL);
    xTaskCreate(vDoorOpenCloseTask, "DoorOpenClose", 128, NULL, 3, NULL);
    xTaskCreate(vSpeedTask, "Speed", 128, NULL, 3, NULL);
    xTaskCreate(vGearTask, "Gear", 128, NULL, 3, NULL);
    xTaskCreate(vDisplayTask, "Display", 128, NULL, 2, NULL);
    xTaskCreate(vUltrasonicTask, "Ultrasonic", 128, NULL, 3, NULL);
    xTaskCreate(vIgnitionStatusTask, "IgnitionStatus", 128, NULL, 3, NULL);
    
    // Start scheduler
    vTaskStartScheduler();
    
    // Should never reach here
    while(1);
}
