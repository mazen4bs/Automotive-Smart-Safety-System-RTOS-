#include "ultrasonic_system.h"
#include "TM4C123GH6PM.h"
#include "gear_system.h"

// Global variables
static float currentDistance = 0.0f;
static uint32_t lastMeasurementTime = 0;

// Initialize ultrasonic sensor and LED pins
void UltrasonicSystem_Init(void) {
    // Enable GPIO ports
    SYSCTL->RCGCGPIO |= (1 << 2) | (1 << 3) | (1 << 4); // Enable GPIOC, GPIOD, GPIOE
    while((SYSCTL->PRGPIO & ((1 << 2) | (1 << 3) | (1 << 4))) == 0); // Wait for ports to be ready
    
    // Configure trigger pin as output
    TRIGGER_PORT->DIR |= (1 << TRIGGER_PIN);
    TRIGGER_PORT->DEN |= (1 << TRIGGER_PIN);
    TRIGGER_PORT->DATA &= ~(1 << TRIGGER_PIN); // Set trigger low initially
    
    // Configure echo pin as input with pull-down
    ECHO_PORT->DIR &= ~(1 << ECHO_PIN);
    ECHO_PORT->DEN |= (1 << ECHO_PIN);
    ECHO_PORT->PDR |= (1 << ECHO_PIN);  // Enable pull-down resistor
    
    // Configure LED pins as outputs
    GREEN_LED_PORT->DIR |= (1 << GREEN_LED_PIN);
    GREEN_LED_PORT->DEN |= (1 << GREEN_LED_PIN);
    YELLOW_LED_PORT->DIR |= (1 << YELLOW_LED_PIN);
    YELLOW_LED_PORT->DEN |= (1 << YELLOW_LED_PIN);
    RED_LED_PORT->DIR |= (1 << RED_LED_PIN);
    RED_LED_PORT->DEN |= (1 << RED_LED_PIN);
    
    // Configure buzzer pin as output
    GPIOE->DIR |= (1 << 1);  // PE1 as output
    GPIOE->DEN |= (1 << 1);  // Digital enable
    GPIOE->DATA &= ~(1 << 1); // Buzzer off initially
    
    // Turn off all LEDs initially
    GREEN_LED_PORT->DATA &= ~(1 << GREEN_LED_PIN);
    YELLOW_LED_PORT->DATA &= ~(1 << YELLOW_LED_PIN);
    RED_LED_PORT->DATA &= ~(1 << RED_LED_PIN);
    
    // Test trigger pulse
    TRIGGER_PORT->DATA |= (1 << TRIGGER_PIN);
    for(volatile int i = 0; i < 1000; i++); // Delay
    TRIGGER_PORT->DATA &= ~(1 << TRIGGER_PIN);
}

// Measure distance using ultrasonic sensor
static float MeasureDistance(void) {
    uint32_t echoTime = 0;
    float distance = 0.0f;
    uint32_t timeout = 0;
    
    // Send trigger pulse (10us)
    TRIGGER_PORT->DATA &= ~(1 << TRIGGER_PIN);  // Ensure trigger is low
    for(volatile int i = 0; i < 100; i++);      // 10us delay
    TRIGGER_PORT->DATA |= (1 << TRIGGER_PIN);   // Set trigger high
    for(volatile int i = 0; i < 100; i++);      // 10us delay
    TRIGGER_PORT->DATA &= ~(1 << TRIGGER_PIN);  // Set trigger low
    
    // Wait for echo to go high with timeout
    timeout = 0;
    while(!(ECHO_PORT->DATA & (1 << ECHO_PIN))) {
        timeout++;
        if(timeout > 100000) {  // Reduced timeout to 100ms
            return 0.0f;  // Return 0 if no echo received
        }
    }
    
    // Reset echo time counter
    echoTime = 0;
    
    // Wait for echo to go low with timeout
    timeout = 0;
    while(ECHO_PORT->DATA & (1 << ECHO_PIN)) {
        echoTime++;
        timeout++;
        if(timeout > 100000) {  // Reduced timeout to 100ms
            return 0.0f;  // Return 0 if echo doesn't go low
        }
    }
    
    // Calculate distance (speed of sound = 340 m/s)
    // distance = (time * speed) / 2
    // time in microseconds, speed in cm/microsecond
    distance = (echoTime * 0.034) / 2;
    
    // Limit maximum distance to 150cm
    if(distance > 150.0f) distance = 150.0f;
    
    return distance;
}

// Get current distance
float UltrasonicSystem_GetDistance(void) {
    return currentDistance;
}

// Update distance measurement
void UltrasonicSystem_Update(void) {
    // Only measure distance when in reverse gear
    if(GearSystem_GetCurrentGear() == GEAR_REVERSE) {
        currentDistance = MeasureDistance();
        lastMeasurementTime = xTaskGetTickCount();
    } else {
        currentDistance = 0.0f;
    }
}

// Turn off all LEDs
void UltrasonicSystem_TurnOffLEDs(void) {
    GREEN_LED_PORT->DATA &= ~(1 << GREEN_LED_PIN);
    YELLOW_LED_PORT->DATA &= ~(1 << YELLOW_LED_PIN);
    RED_LED_PORT->DATA &= ~(1 << RED_LED_PIN);
}

// Turn off buzzer
void UltrasonicSystem_TurnOffBuzzer(void) {
    GPIOE->DATA &= ~(1 << 1); // Turn off PE1 (buzzer)
}

// Update LED indicators based on distance
void UltrasonicSystem_UpdateLEDs(float distance) {
    // Turn off all LEDs first
    GREEN_LED_PORT->DATA &= ~(1 << GREEN_LED_PIN);
    YELLOW_LED_PORT->DATA &= ~(1 << YELLOW_LED_PIN);
    RED_LED_PORT->DATA &= ~(1 << RED_LED_PIN);
    
    // Update LEDs based on distance
    if(distance > SAFE_DISTANCE) {
        GREEN_LED_PORT->DATA |= (1 << GREEN_LED_PIN);
    } else if(distance > CAUTION_DISTANCE) {
        YELLOW_LED_PORT->DATA |= (1 << YELLOW_LED_PIN);
    } else if(distance > 0.0f) {  // Only turn on red LED if we have a valid reading
        RED_LED_PORT->DATA |= (1 << RED_LED_PIN);
    }
}

// Update buzzer based on distance
void UltrasonicSystem_UpdateBuzzer(float distance) {
    static uint32_t lastBeepTime = 0;
    uint32_t currentTime = xTaskGetTickCount();
    uint32_t beepInterval;
    
    // Only beep if we have a valid reading
    if(distance <= 0.0f) {
        GPIOE->DATA &= ~(1 << 1); // Turn off buzzer
        return;
    }
    
    // Calculate beep interval based on distance
    if(distance > SAFE_DISTANCE) {
        beepInterval = 1000; // 1 second when safe
    } else if(distance > CAUTION_DISTANCE) {
        beepInterval = 500; // 0.5 seconds in caution zone
    } else {
        beepInterval = 200; // 0.2 seconds in danger zone
    }
    
    // Toggle buzzer based on interval
    if(currentTime - lastBeepTime >= pdMS_TO_TICKS(beepInterval)) {
        GPIOE->DATA ^= (1 << 1); // Toggle PE1 (buzzer)
        lastBeepTime = currentTime;
    }
} 