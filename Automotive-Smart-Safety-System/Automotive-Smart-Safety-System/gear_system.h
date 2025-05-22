#ifndef GEAR_SYSTEM_H
#define GEAR_SYSTEM_H

#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

// Gear definitions
typedef enum {
    GEAR_DRIVE,
    GEAR_REVERSE,
		GEAR_PARK
} Gear_t;

// Function declarations
void GearSystem_Init(void);
uint8_t GearSystem_Update(void);  // Returns 1 if gear changed, 0 if not
Gear_t GearSystem_GetCurrentGear(void);

// External declarations
extern SemaphoreHandle_t xLCDMutex;

#endif // GEAR_SYSTEM_H 