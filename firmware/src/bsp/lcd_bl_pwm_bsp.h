#ifndef LCD_BL_PWM_BSP_H
#define LCD_BL_PWM_BSP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Backlight LED runs active-low on the LEDC channel: brightness 255 = full,
 * 0 = dark. The duty inversion is handled inside this driver. */
void lcd_bl_pwm_bsp_init(uint8_t brightness);
void lcd_bl_set_brightness(uint8_t brightness);

#ifdef __cplusplus
}
#endif

#endif
