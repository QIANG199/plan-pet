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
  String petAgent;
  String petState;
  bool fresh;
};

void net_begin();
void net_reconnect();
void net_loop();
bool net_wifi_up();
const Snapshot &net_snapshot();
