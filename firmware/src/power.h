#pragma once

#include <stdint.h>

/* Phases published to the UI (written by the power task, read by the UI loop). */
enum PowerPhase : uint8_t {
  POWER_NORMAL = 0,     /* no key activity */
  POWER_HOLD_OFF = 1,   /* PWR held, 0..1000 per-mille toward the 3s trigger */
  POWER_COUNTDOWN = 2,  /* shutdown countdown, remain seconds */
  POWER_HOLD_REBOOT = 3 /* BOOT held past 1s, remain seconds to the 5s trigger */
};

enum KeyRequest : uint8_t {
  KEY_REQ_NONE = 0,
  KEY_REQ_THEME = 1,    /* BOOT single click */
  KEY_REQ_SETTINGS = 2  /* BOOT double click */
};

void power_begin();
void power_relatch();
void power_read(bool *charging, int *pct);

PowerPhase power_phase();
uint16_t power_hold_permille();   /* 0..1000 while POWER_HOLD_OFF */
uint8_t power_countdown_remain(); /* seconds while POWER_COUNTDOWN */
uint8_t power_reboot_remain();    /* seconds while POWER_HOLD_REBOOT */
uint8_t power_take_request();     /* consume one pending KeyRequest */
void power_request_shutdown();    /* trigger the countdown without the key */
