#pragma once

#include <Arduino.h>
#include <time.h>

struct QuotaBar {
  bool present;
  int pct;
  time_t resetAt;
};

struct Snapshot {
  bool glmOk;
  QuotaBar h5;
  QuotaBar week;
  bool cursorOk;
  QuotaBar autoBar;
  QuotaBar apiBar;
  time_t cycleEnd;
  time_t srvTs;      /* server unix seconds of this snapshot */
  time_t petSince;   /* pet lane's last state switch, unix seconds */
  String petAgent;
  String petState;
  bool glmDot;
  bool curDot;
  bool fresh;
};

void net_begin();
void net_reconnect();
void net_loop();
bool net_wifi_up();
const Snapshot &net_snapshot();
uint32_t net_last_ok_ms();          /* millis() of the last parsed snapshot (boot time if none) */
bool net_server_ok();               /* WiFi up and a snapshot parsed recently */
void net_set_poll_ms(uint32_t ms);  /* poll cadence; 1s awake, slower while the screen sleeps */
void net_sleep_enter();             /* screen off: radio duty-cycled in short windows */
void net_sleep_exit();              /* screen on: keep WiFi up and poll normally */
