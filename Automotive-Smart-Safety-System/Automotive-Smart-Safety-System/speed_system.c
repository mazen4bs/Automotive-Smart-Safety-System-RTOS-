#include "speed_system.h"
#include "TM4C123GH6PM.h"
#include "lcd.h"
#include "gear_system.h"
#include "Door.h"
#include <stdio.h>

// Global variables
static float currentSpeed = 0.0f;
static uint32_t lastADCValue = 0;
static uint32_t lastUpdateTime = 0;
static uint32_t minADCValue = 4095;  // Track minimum ADC value
static uint32_t maxADCValue = 0;     // Track maximum ADC value

// Speed limits
#define REVERSE_SPEED_LIMIT 30.0f  // 30 km/h limit in reverse
#define MAX_SPEED 100.0f          // Maximum speed in km/h

// Initialize ADC for potentiometer
void SpeedSystem_Init(void) {
    // Enable ADC0 and GPIOE peripherals
    SYSCTL->RCGCADC |= (1 << 0);  // Enable ADC0
    SYSCTL->RCGCGPIO |= (1 << 4); // Enable GPIOE
    while((SYSCTL->PRGPIO & (1 << 4)) == 0); // Wait for GPIOE to be ready
    
    // Configure PE3 as ADC input
    GPIOE->AFSEL |= (1 << 3);     // Enable alternate function
    GPIOE->DEN &= ~(1 << 3);      // Disable digital function
    GPIOE->AMSEL |= (1 << 3);     // Enable analog function
    
    // Configure ADC0
    ADC0->ACTSS &= ~(1 << 0);     // Disable sample sequencer 0
    ADC0->EMUX &= ~(0xF << 0);    // Software trigger
    ADC0->SSMUX0 = 0;             // Input channel 0 (AIN0)
    ADC0->SSCTL0 = (1 << 1) |     // End of sequence
                   (1 << 2);       // Interrupt enable
    ADC0->ACTSS |= (1 << 0);      // Enable sample sequencer 0
}

// Calculate speed based on potentiometer value
static float CalculateSpeed(uint32_t adcValue) {
    float speed;
    
    // If ignition is off, force speed to 0
    if (!DoorSystem_IsIgnitionOn()) {
        return 0.0f;
    }
    
    // Update min/max ADC values
    if (adcValue < minADCValue) minADCValue = adcValue;
    if (adcValue > maxADCValue) maxADCValue = adcValue;
    
    // Calculate speed using the actual range of the potentiometer
    if (maxADCValue > minADCValue) {
        // Map the ADC value to speed using the actual range
        speed = ((float)(adcValue - minADCValue) * MAX_SPEED) / (maxADCValue - minADCValue);
    } else {
        speed = 0.0f;
    }
    
    // Apply speed limit if in reverse gear
    if (GearSystem_GetCurrentGear() == GEAR_REVERSE && speed > REVERSE_SPEED_LIMIT) {
        speed = REVERSE_SPEED_LIMIT;
    }
    
    // Force speed to 0 if in PARK gear
    if (GearSystem_GetCurrentGear() == GEAR_PARK) {
        speed = 0.0f;
    }
    
    return speed;
}

// Update speed reading and display
void SpeedSystem_Update(void) {
    uint32_t adcValue;
    
    // If ignition is off, keep speed at 0
    if (!DoorSystem_IsIgnitionOn()) {
        currentSpeed = 0.0f;
        return;
    }
    
    // Trigger ADC conversion
    ADC0->PSSI |= (1 << 0);       // Start conversion on sequencer 0
    
    // Wait for conversion to complete
    while((ADC0->RIS & (1 << 0)) == 0);
    
    // Read ADC value
    adcValue = ADC0->SSFIFO0;
    
    // Clear interrupt
    ADC0->ISC = (1 << 0);
    
    // Calculate speed
    currentSpeed = CalculateSpeed(adcValue);
}

// Get current speed
float SpeedSystem_GetCurrentSpeed(void) {
    return currentSpeed;
} 