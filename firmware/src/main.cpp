#include <Arduino.h>
#include "user_config.h"
#include "lvgl_port.h"
#include "i2c_bsp.h"
#include "lcd_bl_bsp/lcd_bl_pwm_bsp.h"
#include "config.h"
#include "net.h"
#include "ui.h"

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("desktop-pet firmware");
  Serial.printf("psram %u bytes\n", ESP.getPsramSize());
  i2c_master_Init();
  lvgl_port_init();
  lcd_bl_pwm_bsp_init(LCD_PWM_MODE_255);
  config_begin();
  if (lvgl_port_lock(-1)) {
    ui_create();
    lvgl_port_unlock();
  }
  net_begin();
}

void loop() {
  config_poll_serial();
  net_loop();
  static uint32_t lastClock;
  if (millis() - lastClock > 1000) {
    lastClock = millis();
    if (lvgl_port_lock(20)) {
      ui_tick_clock();
      lvgl_port_unlock();
    }
  }
  delay(10);
}
