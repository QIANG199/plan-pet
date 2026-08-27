#pragma once

#include "net.h"

void ui_create();
void ui_apply(const Snapshot &s);
void ui_set_link(bool wifiUp, bool srvOk); /* wifi icon: red=WiFi down, amber=server lost, muted=ok */
void ui_set_glm_peak(bool on);             /* GLM peak-pricing badge "• 3x" next to the title */
void ui_set_sleep(bool on);                /* blank screen / restore with a 300ms fade */
void ui_set_power(bool charging, int pct);
void ui_tick_clock();
void ui_set_pet_override(const char *state);
void ui_toggle_theme();
void ui_poll_power();
