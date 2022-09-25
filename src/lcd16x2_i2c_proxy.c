#include "lcd16x2_i2c_proxy.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>

/* LCD 16x2 control pins positions on IO Expander */
#define PCF8574_LCD_RS_PIN        (1 << 0)
#define PCF8574_LCD_RW_PIN        (1 << 1)
#define PCF8574_LCD_EN_PIN    	  (1 << 2)
#define PCF8574_LCD_BKL_PIN       (1 << 3)
// D7 to D4 are located in pins 7 to 4

/*HD44780 commands and subcommands bitfields*/
#define LCD_COMMAND_CLEAR                   0x01
#define LCD_COMMAND_HOME     		        0x02
#define LCD_COMMAND_SET_ENTRY_MODE          0x04
    #define LCD_ENTRY_MODE_SHIFT                  0x01
    #define LCD_ENTRY_MODE_NO_SHIFT               0x00
    #define LCD_ENTRY_MODE_ID_INCREMENT           0x02
    #define LCD_ENTRY_MODE_ID_DECREMENT           0x00

#define LCD_COMMAND_ON_OFF_CONTROL          0x08
    #define LCD_ON_OFF_CONTROL_DISPLAY_ON         0x04
    #define LCD_ON_OFF_CONTROL_DISPLAY_OFF        0x00
    #define LCD_ON_OFF_CONTROL_CURSOR_ON          0x02
    #define LCD_ON_OFF_CONTROL_CURSOR_OFF         0x00
    #define LCD_ON_OFF_CONTROL_BLINK_CURSOR_ON    0x01
    #define LCD_ON_OFF_CONTROL_BLINK_CURSOR_OFF   0x00

#define LCD_COMMAND_SHIFT                   0x10
    #define LCD_SHIFT_RIGHT_TO_LEFT               0x04
    #define LCD_SHIFT_LEFT_TO_RIGHT               0x00
    #define LCD_SHIFT_SC_DISPLAY_SHIFT            0x08
    #define LCD_SHIFT_SC_CURSOR_MOVE              0x00

#define LCD_COMMAND_FUNCTION_SET            0x20
    #define LCD_FUNCTION_F_5_DOT_10_CHAR      	  0x04
    #define LCD_FUNCTION_F_5_DOT_8_CHAR           0x00
    #define LCD_FUNCTION_N_2_LINES                0x08
    #define LCD_FUNCTION_N_1_LINES    		      0x00
    #define LCD_FUNCTION_DL_8BITS_INTERFACE       0x10
    #define LCD_FUNCTION_DL_4BITS_INTERFACE       0x00

#define LCD_COMMAND_SET_DDRAM_ADDR			      0x80
// used to set cursor position

/* Class variables */
static I2C_HandleTypeDef* lcd16x2_i2c_proxy_pI2cHandle;
static bool lcd16x2_i2c_proxy_cursorOn = false;
static bool lcd16x2_i2c_proxy_blinking = false;

/*
 * @brief Send nibble as command (RS = 0) to HD44780. This function is used in the initialization, when
 * some nibbles must be sent through D7-D4 according to IC datasheet
 * @param[in] nibbleOnLSN  A byte containing the nibble to be sent in the less significant nibble. example: 0x03 to send 0x3 through D7-D4
 * @return -1 if I2C transmission fails, 0 if succeeds
 */
static int32_t lcd16x2_i2c_proxy_sendCommandNibble(const uint8_t nibbleOnLSN)
{
    const uint8_t dataD7ToD4 = (0xF0 & (nibbleOnLSN << 4));
    uint8_t i2cData[2] =
        {
            dataD7ToD4 | PCF8574_LCD_BKL_PIN | PCF8574_LCD_EN_PIN,
            dataD7ToD4 | PCF8574_LCD_BKL_PIN,
        };
    if (HAL_I2C_Master_Transmit(lcd16x2_i2c_proxy_pI2cHandle, LCD_I2C_SLAVE_ADDRESS, i2cData, 2, 100) != HAL_OK)
    {
        return -1;
    }
    else
    {
        return 0;
    }
}

/*
 * @brief Send byte command to HD44780. Most significant nibble is D7-D4 and less significant nibble is D3-D0.
 * @param[in] command  command to be sent
 * @return -1 if I2C transmission fails, 0 if succeeds
 */
static int32_t lcd16x2_i2c_proxy_sendCommand(uint8_t command)
{
    const uint8_t dataD7ToD4 = (0xF0 & (command));
    const uint8_t dataD3ToD0 = (0xF0 & (command << 4));
    uint8_t i2cData[4] =
        {
            dataD7ToD4 | PCF8574_LCD_BKL_PIN | PCF8574_LCD_EN_PIN,
            dataD7ToD4 | PCF8574_LCD_BKL_PIN,
            dataD3ToD0 | PCF8574_LCD_BKL_PIN | PCF8574_LCD_EN_PIN,
            dataD3ToD0 | PCF8574_LCD_BKL_PIN,
        };
    if (HAL_I2C_Master_Transmit(lcd16x2_i2c_proxy_pI2cHandle, LCD_I2C_SLAVE_ADDRESS, i2cData, 4, 100) != HAL_OK)
    {
        return -1;
    }
    else
    {
        return 0;
    }
}

/*
 * @brief Send data to be written to display by HD44780. Most significant nibble is D7-D4 and less significant nibble is D3-D0.
 * @param[in] data data to be written (ASCII code)
 * @return -1 if I2C transmission fails, 0 if succeeds
 */
static int32_t lcd16x2_i2c_proxy_sendData(uint8_t data)
{
    const uint8_t dataD7ToD4 = (0xF0 & (data));
    const uint8_t dataD3ToD0 = (0xF0 & (data << 4));
    uint8_t i2cData[4] =
        {
            dataD7ToD4 | PCF8574_LCD_BKL_PIN | PCF8574_LCD_RS_PIN | PCF8574_LCD_EN_PIN,
            dataD7ToD4 | PCF8574_LCD_BKL_PIN | PCF8574_LCD_RS_PIN,
            dataD3ToD0 | PCF8574_LCD_BKL_PIN | PCF8574_LCD_RS_PIN | PCF8574_LCD_EN_PIN,
            dataD3ToD0 | PCF8574_LCD_BKL_PIN | PCF8574_LCD_RS_PIN,
        };
    if (HAL_I2C_Master_Transmit(lcd16x2_i2c_proxy_pI2cHandle, LCD_I2C_SLAVE_ADDRESS, i2cData, 4, 100) != HAL_OK)
    {
        return -1;
    }
    else
    {
        return 0;
    }
}

/*
 * @brief Initialize LCD 16x2 through PCF8574 and HD44780
 * @param[in] pI2cHandle  pointer to HAL I2C handle. The maximum frequency supported by PCF8574 is 100kHz according to datasheet.
 * @param showCursor  enables cursor to be shown in the writing position
 * @param blinkCursor  enables cursor blinking effect
 * @return -1 if initialization fails, 0 if succeeds
 */
int32_t lcd16x2_i2c_proxy_initialize(I2C_HandleTypeDef *pI2cHandle, bool showCursor, bool blinkCursor)
{
    int32_t transmissionResult = 0;

    lcd16x2_i2c_proxy_pI2cHandle = pI2cHandle;
    lcd16x2_i2c_proxy_cursorOn = showCursor;
    lcd16x2_i2c_proxy_blinking = blinkCursor;

    if (HAL_I2C_IsDeviceReady(lcd16x2_i2c_proxy_pI2cHandle, LCD_I2C_SLAVE_ADDRESS, 5, 500) != HAL_OK)
    {
        return -1;
    }

    // Delays and commands sequence specified in HD44780 datasheet
    HAL_Delay(45);
    transmissionResult |= lcd16x2_i2c_proxy_sendCommandNibble(0x03);
    HAL_Delay(5);
    transmissionResult |= lcd16x2_i2c_proxy_sendCommandNibble(0x03);
    HAL_Delay(1);
    transmissionResult |= lcd16x2_i2c_proxy_sendCommandNibble(0x03);
    HAL_Delay(1);
    transmissionResult |= lcd16x2_i2c_proxy_sendCommandNibble(0x02);
    HAL_Delay(1);
    // Initial configuration
    transmissionResult |= lcd16x2_i2c_proxy_sendCommand(LCD_COMMAND_FUNCTION_SET | LCD_FUNCTION_DL_4BITS_INTERFACE | LCD_FUNCTION_F_5_DOT_8_CHAR | LCD_FUNCTION_N_2_LINES);
    HAL_Delay(1);
    transmissionResult |= lcd16x2_i2c_proxy_sendCommand(LCD_COMMAND_ON_OFF_CONTROL |
                                                        LCD_ON_OFF_CONTROL_DISPLAY_ON |
                                                        (lcd16x2_i2c_proxy_cursorOn ? LCD_ON_OFF_CONTROL_CURSOR_ON : LCD_ON_OFF_CONTROL_CURSOR_OFF) |
                                                        (lcd16x2_i2c_proxy_blinking ? LCD_ON_OFF_CONTROL_BLINK_CURSOR_ON : LCD_ON_OFF_CONTROL_BLINK_CURSOR_OFF));
    HAL_Delay(1);
    transmissionResult |= lcd16x2_i2c_proxy_sendCommand(LCD_COMMAND_CLEAR);
    HAL_Delay(1);
    transmissionResult |= lcd16x2_i2c_proxy_sendCommand(LCD_COMMAND_SET_ENTRY_MODE | LCD_ENTRY_MODE_ID_INCREMENT | LCD_ENTRY_MODE_NO_SHIFT);
    if (transmissionResult != HAL_OK)
    {
        return -1;
    }
    return 0;
}

/*
 * @brief Turn on display in the last state before turning off
 * @return -1 if i2c transmission fails, 0 if succeeds
 */
int32_t lcd16x2_i2c_proxy_turnDisplayOn()
{
    return lcd16x2_i2c_proxy_sendCommand(LCD_COMMAND_ON_OFF_CONTROL |
                                         LCD_ON_OFF_CONTROL_DISPLAY_ON |
                                         (lcd16x2_i2c_proxy_cursorOn ? LCD_ON_OFF_CONTROL_CURSOR_ON : LCD_ON_OFF_CONTROL_CURSOR_OFF) |
                                         (lcd16x2_i2c_proxy_blinking ? LCD_ON_OFF_CONTROL_BLINK_CURSOR_ON : LCD_ON_OFF_CONTROL_BLINK_CURSOR_OFF));
}

/*
 * @brief Turn off display. Display will now show any character but will keep state for the next turn on command.
 * @return -1 if i2c transmission fails, 0 if succeeds
 */
int32_t lcd16x2_i2c_proxy_turnDisplayOff()
{
    return lcd16x2_i2c_proxy_sendCommand(LCD_COMMAND_ON_OFF_CONTROL |
                                         LCD_ON_OFF_CONTROL_DISPLAY_OFF |
                                         LCD_ON_OFF_CONTROL_CURSOR_OFF |
                                         LCD_ON_OFF_CONTROL_BLINK_CURSOR_OFF);
}

/*
 */
int32_t lcd16x2_i2c_proxy_setCursor(bool cursorOn, bool blinking)
{
    lcd16x2_i2c_proxy_cursorOn = cursorOn;
    lcd16x2_i2c_proxy_blinking = blinking;
    return lcd16x2_i2c_proxy_sendCommand(LCD_COMMAND_ON_OFF_CONTROL |
                                         LCD_ON_OFF_CONTROL_DISPLAY_ON |
                                         (lcd16x2_i2c_proxy_cursorOn ? LCD_ON_OFF_CONTROL_CURSOR_ON : LCD_ON_OFF_CONTROL_CURSOR_OFF) |
                                         (lcd16x2_i2c_proxy_blinking ? LCD_ON_OFF_CONTROL_BLINK_CURSOR_ON : LCD_ON_OFF_CONTROL_BLINK_CURSOR_OFF));
}

/**
 * @brief Set cursor to first position
 * @param[in] row 0 for first row and 1 for second row
 * @return -1 if i2c transmission fails, 0 if succeeds
 */
int32_t lcd16x2_i2c_proxy_setCursorHome()
{
    return lcd16x2_i2c_proxy_sendCommand(LCD_COMMAND_HOME);
}

/**
 * @brief Set cursor position
 * @param[in] row 0 for first row and 1 for second row
 * @param[in] col 0 to 15 for first to 16th column
 * @return -1 if i2c transmission fails, 0 if succeeds
 */
int32_t lcd16x2_i2c_proxy_setCursorPosition(uint8_t row, uint8_t col)
{
    if ((row > 1) || (col > 15))
    {
        return -1;
    }
    return lcd16x2_i2c_proxy_sendCommand(LCD_COMMAND_SET_DDRAM_ADDR | ((row & 0x01) << 6) | (col & 0x0F));
}

/**
 * @brief Clear display
 * @return -1 if i2c transmission fails, 0 if succeeds
 */
int32_t lcd16x2_i2c_proxy_clear(void)
{
    return lcd16x2_i2c_proxy_sendCommand(LCD_COMMAND_CLEAR);
}

/**
 * @brief Shift content to the left, revealing right side of display buffer
 * @param[in] offset how many steps to the left
 * @return -1 if i2c transmission fails, 0 if succeeds
 */
int32_t lcd16x2_i2c_proxy_scrollRight(uint8_t offset)
{
    for (uint8_t i = 0; i < offset; i++)
    {
        HAL_Delay(1);
        lcd16x2_i2c_proxy_sendCommand(LCD_COMMAND_SHIFT | LCD_SHIFT_SC_DISPLAY_SHIFT | LCD_SHIFT_LEFT_TO_RIGHT);
    }
    return 0;
}

/**
 * @brief Shift content to the right, revealing left side of display buffer
 * @param[in] offset how many steps to the left
 * @return -1 if i2c transmission fails, 0 if succeeds
 */
int32_t lcd16x2_i2c_proxy_scrollLeft(uint8_t offset)
{
    for (uint8_t i = 0; i < offset; i++)
    {
        HAL_Delay(1);
        lcd16x2_i2c_proxy_sendCommand(LCD_COMMAND_SHIFT | LCD_SHIFT_SC_DISPLAY_SHIFT | LCD_SHIFT_RIGHT_TO_LEFT);
    }
    return 0;
}

/**
 * @brief Print char to screen
 * @param[in] char to be written
 * @return -1 if i2c transmission fails, 0 if succeeds
 */
int32_t lcd16x2_i2c_proxy_printc(char c)
{
    return lcd16x2_i2c_proxy_sendData(c);
}

/**
 * @brief Print formatted string to screen.
 * @param[in] str formatted string to be written.  If '\\n' is found, characters will start printing in second row.
 * 	There should be only one '\\n' at maximum. If more than 16 characters are given with no '\\n', 17th character ahead will
 * 	be printed in second row.
 * @param[in] ... string args
 * @return -1 if i2c transmission fails, 0 if succeeds
 */
int32_t lcd16x2_i2c_proxy_printf(const char *str, ...)
{
    char formattedStr[33];
    bool secondLine = false;
    va_list args;
    va_start(args, str);
    vsprintf(formattedStr, str, args);
    va_end(args);
    lcd16x2_i2c_proxy_clear();
    lcd16x2_i2c_proxy_setCursorPosition(0, 0);
    for (uint8_t i = 0; (i < strlen(formattedStr)) && (i < 32); i++)
    {
        HAL_Delay(1);
        if(!secondLine)
        {
            if (formattedStr[i] == '\n')
            {
                secondLine = true;
                lcd16x2_i2c_proxy_setCursorPosition(1, 0);
                continue;
            }
            else if (i == 16)
            {
                secondLine = true;
                lcd16x2_i2c_proxy_setCursorPosition(1, 0);
            }
        }
        lcd16x2_i2c_proxy_sendData((uint8_t)formattedStr[i]);
    }
    return 0;
}

int32_t printToRow(const uint8_t row, const char *str, ...)
{
    char formattedStr[33];
    uint8_t strSize;
    if(row > 1)
    {
        return -1;
    }
    va_list args;
    va_start(args, str);
    vsprintf(formattedStr, str, args);
    va_end(args);
    lcd16x2_i2c_proxy_setCursorPosition(row, 0);
    strSize = strlen(formattedStr);
    for (uint8_t i = 0; (i < strSize) && (i < 16); i++)
    {
        lcd16x2_i2c_proxy_sendData((uint8_t)formattedStr[i]);
    }
    for (int8_t i = 16-strSize; i > 0; i--)
    {
        lcd16x2_i2c_proxy_sendData(' ');
    }
    return 0;
}
