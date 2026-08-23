#pragma once

#include "net.h"

void ui_create();
void ui_apply(const Snapshot &s);
void ui_set_wifi(bool up);
void ui_set_power(bool charging, int pct);
void ui_tick_clock();
void ui_set_pet_override(const char *state);
