#include "power.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <Arduino.h>
#include "HWCDC.h"
#include "lcd_bl_bsp/lcd_bl_pwm_bsp.h"

static const int PIN_VBAT = 4;
static const int PIN_PWR = 16;
static const float V_EMPTY = 3.40f;
static const float V_FULL = 4.20f;
static const float V_PRESENT = 3.00f;
static const float V_USB_LIFT = 0.11f;
static const uint32_t PWR_HOLD_MS = 3000;
static const uint32_t PWR_RELEASE_MS = 150;

static float vEma;
static bool emaReady;
static int shownPct = -2;
static i2c_master_dev_handle_t tca;
static uint32_t pwrLowSince;
static uint32_t pwrReleasedSince;
static bool shuttingDown;
static bool pwrArmed;
static bool railHeld;

static bool tca_write(uint8_t reg, uint8_t val) {
  if (!tca) return false;
  uint8_t buf[] = {reg, val};
  return i2c_master_transmit(tca, buf, sizeof(buf), pdMS_TO_TICKS(50)) == ESP_OK;
}

static bool tca_read(uint8_t reg, uint8_t *val) {
  if (!tca || !val) return false;
  return i2c_master_transmit_receive(tca, &reg, 1, val, 1, pdMS_TO_TICKS(50)) == ESP_OK;
}

static bool latch_on() {
  // Output first so EXIO6/7 are high before they become outputs (POR out=0xFF).
  if (!tca_write(0x01, 0xC0)) return false;
  if (!tca_write(0x03, 0x3F)) return false;
  uint8_t dir = 0xFF, out = 0;
  if (!tca_read(0x03, &dir) || !tca_read(0x01, &out)) return false;
  railHeld = ((dir & 0xC0) == 0) && ((out & 0xC0) == 0xC0);
  return railHeld;
}

static void latch_off() {
  setUpduty(LCD_PWM_MODE_0);
  tca_write(0x01, 0x80);
  railHeld = false;
}

static void hold_battery_rail() {
  i2c_master_bus_handle_t bus = nullptr;
  if (i2c_master_get_bus_handle(I2C_NUM_0, &bus) != ESP_OK || !bus) return;
  if (!tca) {
    i2c_device_config_t cfg = {};
    cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    cfg.scl_speed_hz = 300000;
    cfg.device_address = 0x20;
    if (i2c_master_bus_add_device(bus, &cfg, &tca) != ESP_OK) {
      tca = nullptr;
      return;
    }
  }
  for (int i = 0; i < 5 && !latch_on(); i++) delay(10);
}

static void power_task(void *) {
  for (;;) {
    power_poll();
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

void power_begin() {
  pinMode(PIN_VBAT, INPUT);
  pinMode(PIN_PWR, INPUT_PULLUP);
  analogSetPinAttenuation(PIN_VBAT, ADC_11db);
  hold_battery_rail();
  if (digitalRead(PIN_PWR) != LOW) pwrArmed = true;
  xTaskCreatePinnedToCore(power_task, "pwr", 3072, nullptr, 4, nullptr, 1);
}

void power_relatch() { latch_on(); }

void power_poll() {
  if (shuttingDown) return;
  bool pressed = digitalRead(PIN_PWR) == LOW;
  if (!pwrArmed) {
    if (pressed) {
      pwrReleasedSince = 0;
      return;
    }
    if (!pwrReleasedSince) pwrReleasedSince = millis();
    if (millis() - pwrReleasedSince < PWR_RELEASE_MS) return;
    pwrArmed = true;
    return;
  }
  if (HWCDC::isPlugged() || !pressed) {
    pwrLowSince = 0;
    return;
  }
  if (!pwrLowSince) pwrLowSince = millis();
  if (millis() - pwrLowSince < PWR_HOLD_MS) return;
  shuttingDown = true;
  latch_off();
}

void power_read(bool *charging, int *pct) {
  uint32_t mv = 0;
  for (int i = 0; i < 16; i++) mv += analogReadMilliVolts(PIN_VBAT);
  float v = (mv / 16.0f / 1000.0f) * 3.0f;
  if (!emaReady) {
    vEma = v;
    emaReady = true;
  } else {
    vEma = 0.65f * vEma + 0.35f * v;
  }

  if (vEma < V_PRESENT) {
    *charging = true;
    *pct = -1;
    shownPct = -1;
    return;
  }

  bool usb = HWCDC::isPlugged();
  *charging = usb;

  float vs = usb ? (vEma - V_USB_LIFT) : vEma;
  int p = (int)((vs - V_EMPTY) / (V_FULL - V_EMPTY) * 100.0f + 0.5f);
  if (p < 0) p = 0;
  if (p > 100) p = 100;

  if (shownPct < 0 || p - shownPct >= 2 || shownPct - p >= 2) {
    shownPct = p;
  }
  *pct = shownPct;
}
