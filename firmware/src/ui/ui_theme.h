#pragma once

#include "lvgl.h"

struct UiPalette {
  lv_color_t bg, card, ink, ink2, track;
};

extern const UiPalette UI_DARK;
extern const UiPalette UI_LIGHT;

extern const lv_color_t UI_OK;
extern const lv_color_t UI_WARN;
extern const lv_color_t UI_BAD;

const UiPalette &ui_palette();
lv_color_t ui_tone(int pct);
