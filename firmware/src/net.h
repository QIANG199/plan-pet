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
  bool glmPeak; /* GLM 高峰计费时段（北京时间 14–18 点，用量 3 倍） */
  QuotaBar h5;
  QuotaBar week;
  bool cursorOk;
  QuotaBar autoBar;
  QuotaBar apiBar;
  time_t cycleEnd;
  /* OpenCode Go：美元额度的三个窗口（5h / 周 / 月），页屏显 OPEN CODE */
  bool ocOk;
  QuotaBar ocH5;
  QuotaBar ocWeek;
  QuotaBar ocMonth;
  time_t srvTs;      /* server unix seconds of this snapshot */
  time_t otaForceAt; /* 服务端置位的「立刻升级」时刻（0 = 无）；值变化才触发一次 */
  time_t petSince;   /* pet lane's last state switch, unix seconds */
  String petAgent;
  String petState;
  bool glmDot;
  bool curDot;
  bool ocDot; /* OpenCode 页呼吸灯：opencode 最近有会话活动（服务端查会话库写入） */
  bool fresh;
};

void net_begin();
void net_reconnect();
void net_loop();
bool net_wifi_up();
bool net_resolve_host(IPAddress &ip); /* config_host(): literal IP or DNS name */
const Snapshot &net_snapshot();
uint32_t net_last_ok_ms();          /* millis() of the last parsed snapshot (boot time if none) */
bool net_server_ok();               /* WiFi up and a snapshot parsed recently */
void net_set_poll_ms(uint32_t ms);  /* poll cadence; 1s awake, slower while the screen sleeps */
void net_sleep_enter();             /* screen off: radio duty-cycled in short windows */
void net_sleep_exit();              /* screen on: keep WiFi up and poll normally */
