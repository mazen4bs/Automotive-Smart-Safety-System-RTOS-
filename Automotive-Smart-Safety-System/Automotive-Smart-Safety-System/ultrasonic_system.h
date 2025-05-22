#ifndef ULTRASONIC_SYSTEM_H
#define ULTRASONIC_SYSTEM_H

#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

// Pin definitions for ultrasonic sensor
#define TRIGGER_PORT GPIOC
#define TRIGGER_PIN 5
#define ECHO_PORT GPIOC
#define ECHO_PIN 6

// Pin definitions for LED indicators
#define GREEN_LED_PORT GPIOE
#define GREEN_LED_PIN 0
#define YELLOW_LED_PORT GPIOD
#define YELLOW_LED_PIN 6
#define RED_LED_PORT GPIOD
#define RED_LED_PIN 0

// Distance thresholds (in cm)
#define SAFE_DISTANCE 100.0f
#define CAUTION_DISTANCE 30.0f

// Function prototypes
void UltrasonicSystem_Init(void);
float UltrasonicSystem_GetDistance(void);
void UltrasonicSystem_Update(void);
void UltrasonicSystem_UpdateLEDs(float distance);
void UltrasonicSystem_UpdateBuzzer(float distance);
void UltrasonicSystem_TurnOffLEDs(void);
void UltrasonicSystem_TurnOffBuzzer(void);

#endif // ULTRASONIC_SYSTEM_H 