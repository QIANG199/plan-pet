#ifndef USER_CONFIG_H
#define USER_CONFIG_H

/* Board pin map for Waveshare ESP32-S3-Touch-LCD-3.49 (172x640, landscape 640x172). */

#define LCD_HOST SPI3_HOST

/* Peripheral I2C bus 0: TCA9554 IO expander (0x20), PCF85063 RTC (0x51). */
#define PIN_PERI_SCL (GPIO_NUM_48)
#define PIN_PERI_SDA (GPIO_NUM_47)
#define RTC_I2C_ADDR 0x51

/* Touch I2C bus 1 (CST-style raw protocol, see bsp/lvgl_port.c). */
#define PIN_TOUCH_SCL (GPIO_NUM_18)
#define PIN_TOUCH_SDA (GPIO_NUM_17)
#define TOUCH_I2C_ADDR 0x3b

/* QSPI panel + backlight (backlight is active-low on the LEDC channel). */
#define PIN_LCD_CS    (GPIO_NUM_9)
#define PIN_LCD_PCLK  (GPIO_NUM_10)
#define PIN_LCD_DATA0 (GPIO_NUM_11)
#define PIN_LCD_DATA1 (GPIO_NUM_12)
#define PIN_LCD_DATA2 (GPIO_NUM_13)
#define PIN_LCD_DATA3 (GPIO_NUM_14)
#define PIN_LCD_RST   (GPIO_NUM_21)
#define PIN_LCD_BL    (GPIO_NUM_8)

#define LVGL_TICK_PERIOD_MS    5
#define LVGL_TASK_MAX_DELAY_MS 500
#define LVGL_TASK_MIN_DELAY_MS 5
#define LVGL_TASK_STACK_SIZE   (8 * 1024)
#define LVGL_TASK_PRIORITY     2

/* 1 = rotate the portrait panel 90° into 640x172 landscape. */
#define DISPLAY_ROTATED 1

/* Native panel resolution (portrait); UI coordinates are 640x172 after rotation. */
#define LCD_H_RES 172
#define LCD_V_RES 640

#define LVGL_DMA_BUFF_LEN   (LCD_H_RES * 64 * 2)
#define LVGL_FRAME_BUFF_LEN (LCD_H_RES * LCD_V_RES * 2)

#endif
