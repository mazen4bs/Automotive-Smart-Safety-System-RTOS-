#ifndef DOOR_SYSTEM_H
#define DOOR_SYSTEM_H

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "TM4C123.h"

// Door states
typedef enum {
    DOORS_UNLOCKED = 0,
    DOORS_LOCKED = 1
} DoorState_t;

// Door open/closed state
typedef enum {
    DOOR_CLOSED = 0,
    DOOR_OPEN = 1
} DoorOpenState_t;

// Function prototypes
void DoorSystem_Init(void);
uint8_t DoorSystem_Update(void);
DoorState_t DoorSystem_GetState(void);
void DoorSystem_SetState(DoorState_t state);
void DoorSystem_ResetManualOverride(void);
uint8_t DoorSystem_IsIgnitionOn(void);
DoorOpenState_t DoorSystem_GetOpenState(void);
void DoorSystem_SetOpenState(DoorOpenState_t state);

extern SemaphoreHandle_t xLCDMutex;

#endif // DOOR_SYSTEM_H