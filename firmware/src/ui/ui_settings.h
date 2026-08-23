#pragma once

/* On-device provisioning UI (BOOT double-click). All functions that touch
 * widgets must be called with the LVGL lock held; ui_settings_poll() is
 * driven from the Arduino loop and also handles scan/connect progress. */
void ui_settings_open();
void ui_settings_close();
bool ui_settings_active();
void ui_settings_poll();
void ui_settings_refresh_theme();
