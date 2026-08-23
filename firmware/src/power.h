#pragma once

void power_begin();
void power_relatch();
void power_poll();
void power_read(bool *charging, int *pct);
