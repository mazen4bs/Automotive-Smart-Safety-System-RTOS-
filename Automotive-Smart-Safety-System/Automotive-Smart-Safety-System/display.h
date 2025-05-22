#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>

// Structure to hold LCD messages
typedef struct {
    char line1[16];  // First line message
    char line2[17];  // Second line message
} LCD_Message_t;

#endif // DISPLAY_H 