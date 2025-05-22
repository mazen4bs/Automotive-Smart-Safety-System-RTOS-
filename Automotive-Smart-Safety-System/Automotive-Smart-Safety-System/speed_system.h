#ifndef SPEED_SYSTEM_H
#define SPEED_SYSTEM_H

#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

// Speed threshold for auto-lock (in km/h)
#define SPEED_THRESHOLD 10

// Function declarations
void SpeedSystem_Init(void);
void SpeedSystem_Update(void);
float SpeedSystem_GetCurrentSpeed(void);

// External declarations
extern QueueHandle_t xDisplayQueue;
extern SemaphoreHandle_t xLCDMutex;

#endif // SPEED_SYSTEM_H 