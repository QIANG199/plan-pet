#include "lcd_bl_pwm_bsp.h"
#include "esp_err.h"
#include "driver/ledc.h"
#include "user_config.h"

static uint8_t duty_for(uint8_t brightness)
{
  return (uint8_t)(255 - brightness);
}

static void apply_channel(uint8_t brightness)
{
  ledc_channel_config_t ledc_conf =
  {
    .gpio_num = PIN_LCD_BL,
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .channel = LEDC_CHANNEL_1,
    .intr_type = LEDC_INTR_DISABLE,
    .timer_sel = LEDC_TIMER_3,
    .duty = duty_for(brightness),
    .hpoint = 0,
  };
  ESP_ERROR_CHECK_WITHOUT_ABORT(ledc_channel_config(&ledc_conf));
}

void lcd_bl_pwm_bsp_init(uint8_t brightness)
{
  ledc_timer_config_t timer_conf =
  {
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .duty_resolution = LEDC_TIMER_8_BIT,
    .timer_num = LEDC_TIMER_3,
    .freq_hz = 50 * 1000,
    .clk_cfg = LEDC_SLOW_CLK_RC_FAST,
  };
  ESP_ERROR_CHECK_WITHOUT_ABORT(ledc_timer_config(&timer_conf));
  apply_channel(brightness);
}

void lcd_bl_set_brightness(uint8_t brightness)
{
  ESP_ERROR_CHECK_WITHOUT_ABORT(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, duty_for(brightness)));
  ESP_ERROR_CHECK_WITHOUT_ABORT(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1));
}

void lcd_bl_pwm_bsp_off(void)
{
  /* Pin idles high = LED dark on the active-low rail, with no PWM residue. */
  ESP_ERROR_CHECK_WITHOUT_ABORT(ledc_stop(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 1));
}

void lcd_bl_pwm_bsp_on(uint8_t brightness)
{
  apply_channel(brightness);
}
