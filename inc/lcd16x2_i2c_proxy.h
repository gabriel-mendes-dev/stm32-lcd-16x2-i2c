/*
 *  lcd16x2_i2c_proxy.h
 *  Author: Gabriel Mendes Simoni
 *  Created on: June 30th, 2022
 *  Description: this module can be used to control 16x2 LCDs driven by HD44780 through PCF8574 8-bit I/O Expander
 *      with STM32 HAL library - tested with STM32F103R8T6
 *  The PCF8574 are connected in order below:
 *  | D7 | D6 | D5 | D4 | BKL | EN | RW | RS |
 *  thus shall be used with HD44780 4 bits mode
 */

#ifndef LCD16X2_I2C_H_
#define LCD16X2_I2C_H_

#include <stdbool.h>
#include <stdint.h>
#include <main.h>

// Put your PCF8574 I2C address here (shifted one position to the left. Ex: 0x27 << 1 == 0x4E)
#define LCD_I2C_SLAVE_ADDRESS 0x4E

int32_t lcd16x2_i2c_proxy_initialize(I2C_HandleTypeDef* pI2cHandle, bool showCursor, bool blinkCursor);
int32_t lcd16x2_i2c_proxy_clear();
int32_t lcd16x2_i2c_proxy_setCursorHome();
int32_t lcd16x2_i2c_proxy_turnDisplayOff();
int32_t lcd16x2_i2c_proxy_turnDisplayOn();
int32_t lcd16x2_i2c_proxy_setCursor(bool cursorOn, bool blinking);
int32_t lcd16x2_i2c_proxy_scrollLeft(uint8_t offset);
int32_t lcd16x2_i2c_proxy_scrollRight(uint8_t offset);
int32_t lcd16x2_i2c_proxy_setCursorPosition(uint8_t row, uint8_t column);
int32_t lcd16x2_i2c_proxy_printc(char c);
int32_t lcd16x2_i2c_proxy_printf(const char* str, ...);
int32_t printToRow(const uint8_t row, const char *str, ...);

#endif /* LCD16X2_I2C_H_ */
