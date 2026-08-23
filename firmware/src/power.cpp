#include "power.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <Arduino.h>
#include "HWCDC.h"
#include "bsp/lcd_bl_pwm_bsp.h"

static const int PIN_VBAT = 4;
static const int PIN_PWR = 16;   /* power key, active low */
static const int PIN_BOOT = 0;   /* BOOT key, active low, strapping pin */

static const float V_EMPTY = 3.40f;
static const float V_FULL = 4.20f;
static const float V_PRESENT = 3.00f;

static const uint32_t PWR_HOLD_MS = 3000;      /* hold PWR this long to trigger off */
static const uint32_t PWR_RELEASE_MS = 150;    /* boot-time settle before arming PWR */
static const uint32_t OFF_COUNTDOWN_MS = 3000; /* visible countdown before power cut */
static const uint32_t BOOT_HOLD_MS = 5000;     /* hold BOOT this long to reboot */
static const uint32_t BOOT_CLICK_MS = 1000;    /* max press length that counts as a click */
static const uint32_t BOOT_DOUBLE_MS = 300;    /* gap in which a second click is a double */

/* Published UI state: written only from the power task, read from the UI loop. */
static volatile uint8_t uiPhase = POWER_NORMAL;
static volatile uint16_t uiHoldPermille = 0;
static volatile uint8_t uiOffRemain = 0;
static volatile uint8_t uiRebootRemain = 0;

static portMUX_TYPE reqMux = portMUX_INITIALIZER_UNLOCKED;
static volatile uint8_t keyRequest = KEY_REQ_NONE;
static volatile bool forceOffReq = false;

static float vEma;
static bool emaReady;
static int shownPct = -2;
static i2c_master_dev_handle_t tca;
static bool railHeld;
static bool shuttingDown;

/* PWR state */
static bool pwrArmed;
static uint32_t pwrLowSince;
static uint32_t pwrReleasedSince;
static bool offCounting;
static bool offReleased;           /* saw a release after the countdown started */
static uint32_t offDeadline;
static bool offIgnoreUntilRelease; /* cancel press is still down */

/* BOOT state */
static bool bootArmed;
static uint32_t bootDownSince;
static uint32_t lastClickEnd;
static bool secondWindow;          /* waiting for a second click */
static bool secondPressed;         /* second click is down */

struct Debounce {
  bool stable;
  bool candidate;
  uint8_t count;
};

static Debounce pwrDb;
static Debounce bootDb;

/* Accept a level change only after it stays the same for two extra polls (~40ms). */
static void debounce_step(Debounce &d, bool raw) {
  if (raw == d.stable) {
    d.candidate = raw;
    d.count = 0;
    return;
  }
  if (raw == d.candidate) {
    if (++d.count >= 2) {
      d.stable = raw;
      d.count = 0;
    }
  } else {
    d.candidate = raw;
    d.count = 1;
  }
}

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
  lcd_bl_set_brightness(0);
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

static void push_request(uint8_t r) {
  portENTER_CRITICAL(&reqMux);
  if (keyRequest == KEY_REQ_NONE) keyRequest = r;
  portEXIT_CRITICAL(&reqMux);
}

static void request_reboot() {
  /* GPIO0 is a strapping pin: restart only once it reads high again,
   * otherwise the chip would land in the USB download bootloader. */
  Serial.println("[pwr] reboot requested");
  for (int i = 0; i < 10 && digitalRead(PIN_BOOT) == LOW; i++) delay(10);
  if (digitalRead(PIN_BOOT) == LOW) return;
  Serial.println("reboot");
  delay(50);
  ESP.restart();
}

static void begin_countdown(uint32_t now, bool releasedAlready) {
  offCounting = true;
  offReleased = releasedAlready;
  offDeadline = now + OFF_COUNTDOWN_MS;
  uiPhase = POWER_COUNTDOWN;
  uiOffRemain = (uint8_t)((OFF_COUNTDOWN_MS + 999) / 1000);
  Serial.println("off countdown; press PWR again to cancel");
}

static void poll_pwr(uint32_t now, bool pressed) {
  if (forceOffReq) {
    forceOffReq = false;
    if (HWCDC::isPlugged()) {
      Serial.println("usb plugged; won't power off");
      return;
    }
    begin_countdown(now, true);
    return;
  }

  if (offCounting) {
    if (!pressed) offReleased = true;
    if ((offReleased && pressed) || HWCDC::isPlugged()) {
      offCounting = false;
      offIgnoreUntilRelease = true;
      uiPhase = POWER_NORMAL;
      Serial.println("off cancelled");
      return;
    }
    uint32_t left = offDeadline > now ? offDeadline - now : 0;
    uiOffRemain = (uint8_t)((left + 999) / 1000);
    if (left == 0) {
      shuttingDown = true;
      latch_off();
    }
    return;
  }

  if (offIgnoreUntilRelease) {
    if (pressed) return;
    offIgnoreUntilRelease = false;
  }

  if (!pwrArmed) {
    if (pressed) {
      pwrReleasedSince = 0;
      return;
    }
    if (!pwrReleasedSince) pwrReleasedSince = now;
    if (now - pwrReleasedSince < PWR_RELEASE_MS) return;
    pwrArmed = true;
    return;
  }

  if (HWCDC::isPlugged() || !pressed) {
    pwrLowSince = 0;
    if (uiPhase == POWER_HOLD_OFF) {
      uiPhase = POWER_NORMAL;
      uiHoldPermille = 0;
    }
    return;
  }
  if (!pwrLowSince) pwrLowSince = now;
  uint32_t held = now - pwrLowSince;
  if (held >= PWR_HOLD_MS) {
    pwrLowSince = 0;
    begin_countdown(now, false);
    return;
  }
  uiPhase = POWER_HOLD_OFF;
  uiHoldPermille = (uint16_t)(held * 1000 / PWR_HOLD_MS);
}

static void boot_press(uint32_t now) {
  bootDownSince = now;
  if (secondWindow) {
    secondWindow = false;
    secondPressed = true;
  }
}

static void boot_release(uint32_t now) {
  if (!bootDownSince) return;
  uint32_t held = now - bootDownSince;
  bootDownSince = 0;
  if (uiPhase == POWER_HOLD_REBOOT) uiPhase = POWER_NORMAL;

  if (held >= BOOT_HOLD_MS) {
    request_reboot();
    return;
  }
  if (secondPressed) {
    secondPressed = false;
    if (held < BOOT_CLICK_MS) push_request(KEY_REQ_SETTINGS);
    return;
  }
  if (held < BOOT_CLICK_MS) {
    secondWindow = true;
    lastClickEnd = now;
  }
}

static void poll_boot(uint32_t now, bool pressed) {
  if (!bootArmed) {
    if (pressed) return;
    bootArmed = true;
    return;
  }

  if (!pressed) {
    if (secondWindow && now - lastClickEnd > BOOT_DOUBLE_MS && !secondPressed) {
      push_request(KEY_REQ_THEME);
      secondWindow = false;
    }
    return;
  }

  if (bootDownSince && !secondPressed) {
    uint32_t held = now - bootDownSince;
    if (held >= 1000 && held < BOOT_HOLD_MS &&
        (uiPhase == POWER_NORMAL || uiPhase == POWER_HOLD_REBOOT)) {
      uiPhase = POWER_HOLD_REBOOT;
      uiRebootRemain = (uint8_t)((BOOT_HOLD_MS - held + 999) / 1000);
    }
  }
}

static void power_poll();

static void power_task(void *) {
  for (;;) {
    power_poll();
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

void power_begin() {
  pinMode(PIN_VBAT, INPUT);
  pinMode(PIN_PWR, INPUT_PULLUP);
  pinMode(PIN_BOOT, INPUT_PULLUP);
  analogSetPinAttenuation(PIN_VBAT, ADC_11db);
  pwrDb.stable = pwrDb.candidate = digitalRead(PIN_PWR) == LOW;
  bootDb.stable = bootDb.candidate = digitalRead(PIN_BOOT) == LOW;
  hold_battery_rail();
  if (digitalRead(PIN_PWR) != LOW) pwrArmed = true;
  xTaskCreatePinnedToCore(power_task, "pwr", 3072, nullptr, 4, nullptr, 1);
}

void power_relatch() { latch_on(); }

static void power_poll() {
  if (shuttingDown) return;
  uint32_t now = millis();

  debounce_step(pwrDb, digitalRead(PIN_PWR) == LOW);
  poll_pwr(now, pwrDb.stable);

  bool bootPrev = bootDb.stable;
  debounce_step(bootDb, digitalRead(PIN_BOOT) == LOW);
  if (bootDb.stable && !bootPrev) boot_press(now);
  else if (!bootDb.stable && bootPrev) boot_release(now);
  poll_boot(now, bootDb.stable);
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

  /* No USB voltage compensation: measuring this board's divider showed the
   * reading does not lift while charging, and an earlier -0.11V offset made
   * the percentage jump ~13% the moment the cable was pulled. */
  float vs = vEma;
  int p = (int)((vs - V_EMPTY) / (V_FULL - V_EMPTY) * 100.0f + 0.5f);
  if (p < 0) p = 0;
  if (p > 100) p = 100;

  if (shownPct < 0 || p - shownPct >= 2 || shownPct - p >= 2) {
    shownPct = p;
  }
  *pct = shownPct;
}

PowerPhase power_phase() { return (PowerPhase)uiPhase; }
uint16_t power_hold_permille() { return uiHoldPermille; }
uint8_t power_countdown_remain() { return uiOffRemain; }
uint8_t power_reboot_remain() { return uiRebootRemain; }

uint8_t power_take_request() {
  portENTER_CRITICAL(&reqMux);
  uint8_t r = keyRequest;
  keyRequest = KEY_REQ_NONE;
  portEXIT_CRITICAL(&reqMux);
  return r;
}

void power_request_shutdown() { forceOffReq = true; }
