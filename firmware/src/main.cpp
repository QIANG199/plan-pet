#include <Arduino.h>
#include "user_config.h"
#include "lvgl_port.h"
#include "i2c_bsp.h"
#include "lcd_bl_bsp/lcd_bl_pwm_bsp.h"
#include "config.h"
#include "net.h"
#include "ui.h"
#include "power.h"
#include "rtc.h"

void setup() {
  i2c_master_Init();
  power_begin();
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);
  lvgl_port_init();
  lcd_bl_pwm_bsp_init(LCD_PWM_MODE_255);
  config_begin();
  power_relatch();
  if (lvgl_port_lock(-1)) {
    ui_create();
    bool charging = false;
    int pct = 0;
    power_read(&charging, &pct);
    ui_set_power(charging, pct);
    lvgl_port_unlock();
  }
  power_relatch();
  rtc_begin();
  net_begin();
  Serial.println("desktop-pet firmware");
  Serial.printf("psram %u bytes\n", ESP.getPsramSize());
}

void loop() {
  config_poll_serial();
  net_loop();
  static uint32_t lastClock;
  static uint32_t lastPower;
  uint32_t now = millis();
  if (now - lastClock > 1000) {
    lastClock = now;
    if (lvgl_port_lock(20)) {
      ui_tick_clock();
      lvgl_port_unlock();
    }
  }
  if (now - lastPower > 2000) {
    lastPower = now;
    bool charging = false;
    int pct = 0;
    power_read(&charging, &pct);
    if (lvgl_port_lock(20)) {
      ui_set_power(charging, pct);
      lvgl_port_unlock();
    }
  }
  delay(10);
}
