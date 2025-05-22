#ifndef LCD_H
#define LCD_H

#include "FreeRTOS.h"
#include "semphr.h"

// LCD I2C address (0x27 is 7-bit address)
#define LCD_ADDR         0x27

// LCD Commands
#define LCD_CLEAR        0x01
#define LCD_HOME         0x02
#define LCD_ENTRY_MODE   0x06
#define LCD_DISPLAY_ON   0x0C
#define LCD_FUNCTION_SET 0x28
#define LCD_SET_CURSOR   0x80

// LCD Control Bits
#define LCD_RS           0x01
#define LCD_EN           0x04
#define LCD_BACKLIGHT    0x08

// Function Prototypes
void LCD_Init(void);
void LCD_command(unsigned char command);
void LCD_data(unsigned char data);
void LCD_write_string(const char *str);
void LCD_set_cursor(int row, int col);
void LCD_Clear(void);
void LCD_print_int(int value);
void clear_cell(int c, int r);
void I2C1_Init(void);
char I2C1_Write_Multiple(int addr, char mem_addr, int len, char *data);


#endif // LCD_H
