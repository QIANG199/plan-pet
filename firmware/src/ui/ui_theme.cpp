#include "ui_theme.h"
#include "config.h"

const UiPalette UI_DARK = {
    lv_color_hex(0x000000), lv_color_hex(0x000000), lv_color_hex(0xe8ecf3),
    lv_color_hex(0x9aa3b2), lv_color_hex(0x2a303c)};
const UiPalette UI_LIGHT = {
    lv_color_hex(0xe9edf2), lv_color_hex(0xffffff), lv_color_hex(0x1c2430),
    lv_color_hex(0x5a6575), lv_color_hex(0xe4e8ee)};

const lv_color_t UI_OK = lv_color_hex(0x22c55e);
const lv_color_t UI_WARN = lv_color_hex(0xf0805a);
const lv_color_t UI_BAD = lv_color_hex(0xef4444);

const UiPalette &ui_palette() { return config_dark() ? UI_DARK : UI_LIGHT; }

lv_color_t ui_tone(int pct) {
  if (pct >= 90) return UI_BAD;
  if (pct >= 70) return UI_WARN;
  return UI_OK;
}
