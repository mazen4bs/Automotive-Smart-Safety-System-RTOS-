#include "lcd.h"
#include <stdio.h>

static SemaphoreHandle_t lcdMutex = NULL;

static void delay_ms(int ms) {
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
        vTaskDelay(pdMS_TO_TICKS(ms));
    else {
        for (volatile int i = 0; i < ms * 4000; i++);
    }
}

void LCD_command(unsigned char cmd) {
    char data[4];
    char upper = cmd & 0xF0;
    char lower = (cmd << 4) & 0xF0;

    data[0] = upper | LCD_BACKLIGHT | LCD_EN;
    data[1] = upper | LCD_BACKLIGHT;
    data[2] = lower | LCD_BACKLIGHT | LCD_EN;
    data[3] = lower | LCD_BACKLIGHT;

    if (lcdMutex) xSemaphoreTake(lcdMutex, portMAX_DELAY);
    I2C1_Write_Multiple(LCD_ADDR, 0, 4, data);
    if (lcdMutex) xSemaphoreGive(lcdMutex);

    delay_ms(2);
}

void LCD_data(unsigned char data_char) {
    char data[4];
    char upper = data_char & 0xF0;
    char lower = (data_char << 4) & 0xF0;

    data[0] = upper | LCD_BACKLIGHT | LCD_RS | LCD_EN;
    data[1] = upper | LCD_BACKLIGHT | LCD_RS;
    data[2] = lower | LCD_BACKLIGHT | LCD_RS | LCD_EN;
    data[3] = lower | LCD_BACKLIGHT | LCD_RS;

    if (lcdMutex) xSemaphoreTake(lcdMutex, portMAX_DELAY);
    I2C1_Write_Multiple(LCD_ADDR, 0, 4, data);
    if (lcdMutex) xSemaphoreGive(lcdMutex);

    delay_ms(1);
}

void LCD_Init(void) {
    I2C1_Init();

    if (lcdMutex == NULL)
        lcdMutex = xSemaphoreCreateMutex();

    delay_ms(50);

    // Initialization sequence
    for (int i = 0; i < 3; i++) {
        char init = 0x30 | LCD_BACKLIGHT;
        I2C1_Write_Multiple(LCD_ADDR, 0, 1, (char[]){init | LCD_EN});
        delay_ms(1);
        I2C1_Write_Multiple(LCD_ADDR, 0, 1, (char[]){init});
        delay_ms(5);
    }

    char init_4bit = 0x20 | LCD_BACKLIGHT;
    I2C1_Write_Multiple(LCD_ADDR, 0, 1, (char[]){init_4bit | LCD_EN});
    delay_ms(1);
    I2C1_Write_Multiple(LCD_ADDR, 0, 1, (char[]){init_4bit});
    delay_ms(5);

    LCD_command(LCD_FUNCTION_SET);
    LCD_command(LCD_DISPLAY_ON);
    LCD_command(LCD_CLEAR);
    LCD_command(LCD_ENTRY_MODE);
    delay_ms(5);
}

void LCD_Clear(void) {
    LCD_command(LCD_CLEAR);
    delay_ms(2);
}

void LCD_set_cursor(int row, int col) {
    const uint8_t offsets[] = {0x00, 0x40};
    if (row > 1) row = 1;
    LCD_command(LCD_SET_CURSOR | (col + offsets[row]));
}

void LCD_write_string(const char *str) {
    while (*str) {
        LCD_data(*str++);
    }
}

void LCD_print_int(int value) {
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%d", value);
    LCD_write_string(buffer);
}

void clear_cell(int c, int r) {
    LCD_set_cursor(r, c);
    LCD_data(' ');
}

void I2C1_Init(void) {
    SYSCTL->RCGCGPIO |= (1 << 0);  // GPIOA
    SYSCTL->RCGCI2C |= (1 << 1);   // I2C1
    while ((SYSCTL->PRGPIO & 0x01) == 0);

    GPIOA->AFSEL |= 0xC0;       // PA6, PA7
    GPIOA->ODR   |= 0x80;       // PA7 open-drain
    GPIOA->DEN   |= 0xC0;       // Digital enable
    GPIOA->PCTL  &= ~0xFF000000;
    GPIOA->PCTL  |= 0x33000000;

    I2C1->MCR = 0x10;           // Master mode
    I2C1->MTPR = 7;             // 100kHz assuming 16MHz
}

static int I2C_wait(void) {
    while (I2C1->MCS & 1); // Busy
    return (I2C1->MCS & 0x0E);
}

char I2C1_Write_Multiple(int addr, char mem_addr, int len, char *data) {
    if (len <= 0) return -1;

    I2C1->MSA = addr << 1;
    I2C1->MDR = mem_addr;
    I2C1->MCS = 3;

    if (I2C_wait()) return -1;

    for (int i = 0; i < len - 1; i++) {
        I2C1->MDR = data[i];
        I2C1->MCS = 1;
        if (I2C_wait()) return -2;
    }

    I2C1->MDR = data[len - 1];
    I2C1->MCS = 5;

    if (I2C_wait()) return -3;

    return 0;
}
