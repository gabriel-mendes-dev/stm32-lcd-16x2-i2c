# stm32-lcd-16x2-i2c
This module can be used with STM32 HAL library to control 16x2 LCDs driven by HD44780 through PCF8574 8-bit I/O Expander.

The PCF8574 I/Os are connected in order below:

| D7 | D6 | D5 | D4 | BKL | EN | RW | RS |

To use this library, just include the .h and .c files to your project and add respective paths to compiler include and source directories.

If you want to use this library in your project and possibly contribute with its development at the same time, I suggest using [git submodules](https://git-scm.com/book/en/v2/Git-Tools-Submodules).
