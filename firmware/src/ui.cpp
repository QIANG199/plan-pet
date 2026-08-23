#include "ui.h"
#include "config.h"
#include "lvgl_port.h"
#include "lvgl.h"
#include <Arduino.h>
#include <time.h>
#include <string.h>

extern "C" const uint8_t pet_frames_bin[];

static const int PET_W = 160;
static const int PET_H = 160;
static const int PET_FRAME_COUNT = 6;
static const int PET_STATE_COUNT = 7;
static const int PET_POKE_IDX = 6;
static const uint32_t PET_FRAME_BYTES = PET_W * PET_H * 2;
static const char *const PET_STATES[] = {
    "idle", "thinking", "typing", "happy", "error", "sleeping", "poke"};

static const lv_color_t COL_OK = lv_color_hex(0x22c55e);
static const lv_color_t COL_WARN = lv_color_hex(0xf0805a);
static const lv_color_t COL_BAD = lv_color_hex(0xef4444);

struct Palette {
  lv_color_t bg, card, ink, ink2, track;
};

static const Palette DARK = {
    lv_color_hex(0x000000), lv_color_hex(0x000000), lv_color_hex(0xe8ecf3),
    lv_color_hex(0x9aa3b2), lv_color_hex(0x2a303c)};
static const Palette LIGHT = {
    lv_color_hex(0xe9edf2), lv_color_hex(0xffffff), lv_color_hex(0x1c2430),
    lv_color_hex(0x5a6575), lv_color_hex(0xe4e8ee)};

static lv_obj_t *scr;
static lv_obj_t *statusCard, *planCard, *petZone;
static lv_obj_t *timeLbl, *rule, *wifiLbl, *battLbl, *battPct;
static lv_obj_t *glmTitle, *glmDot, *glmK1, *glmR1, *glmP1, *glmBar1;
static lv_obj_t *glmK2, *glmR2, *glmP2, *glmBar2;
static lv_obj_t *curTitle, *curDot, *curReset;
static lv_obj_t *curK1, *curP1, *curBar1, *curK2, *curP2, *curBar2;
static lv_obj_t *petImg;
static lv_image_dsc_t petDsc;
static lv_timer_t *petTimer;
static int petFrame;
static int petStateIdx;
static char petFollow[16] = "idle";
static char petOverride[16];
static bool petPoke;
static bool wifiOn = true;

static lv_color_t tone(int pct) {
  if (pct >= 90) return COL_BAD;
  if (pct >= 70) return COL_WARN;
  return COL_OK;
}

static void noScroll(lv_obj_t *o) {
  lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(o, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_style_outline_width(o, 0, 0);
  lv_obj_set_style_min_height(o, 0, 0);
}

static void styleCard(lv_obj_t *o, const Palette &p) {
  noScroll(o);
  lv_obj_set_style_bg_color(o, p.card, 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(o, 8, 0);
  lv_obj_set_style_clip_corner(o, true, 0);
  if (config_dark()) {
    lv_obj_set_style_border_width(o, 1, 0);
    lv_obj_set_style_border_color(o, p.track, 0);
    lv_obj_set_style_border_opa(o, LV_OPA_COVER, 0);
  } else {
    lv_obj_set_style_border_width(o, 0, 0);
  }
}

static lv_obj_t *mkLbl(lv_obj_t *parent, const lv_font_t *font, lv_color_t c) {
  lv_obj_t *l = lv_label_create(parent);
  noScroll(l);
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_color(l, c, 0);
  lv_label_set_long_mode(l, LV_LABEL_LONG_CLIP);
  lv_label_set_text(l, "");
  return l;
}

static lv_obj_t *mkBar(lv_obj_t *parent, const Palette &p) {
  lv_obj_t *b = lv_bar_create(parent);
  noScroll(b);
  lv_obj_set_size(b, lv_pct(100), 7);
  lv_bar_set_range(b, 0, 100);
  lv_obj_set_style_radius(b, 3, 0);
  lv_obj_set_style_bg_color(b, p.track, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(b, 3, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(b, COL_OK, LV_PART_INDICATOR);
  return b;
}

static int pet_index_of(const char *name) {
  for (int i = 0; i < PET_STATE_COUNT; i++) {
    if (strcmp(name, PET_STATES[i]) == 0) return i;
  }
  return 0;
}

static const char *pet_active_name() {
  return petOverride[0] ? petOverride : petFollow;
}

static void pet_show_frame() {
  if (!petImg) return;
  petStateIdx = petPoke ? PET_POKE_IDX : pet_index_of(pet_active_name());
  const int theme = config_dark() ? 0 : 1;
  const uint32_t off =
      ((uint32_t)theme * PET_STATE_COUNT + petStateIdx) * PET_FRAME_COUNT +
      petFrame;
  memset(&petDsc, 0, sizeof(petDsc));
  petDsc.header.magic = LV_IMAGE_HEADER_MAGIC;
  petDsc.header.cf = LV_COLOR_FORMAT_RGB565;
  petDsc.header.w = PET_W;
  petDsc.header.h = PET_H;
  petDsc.header.stride = PET_W * 2;
  petDsc.data_size = PET_FRAME_BYTES;
  petDsc.data = pet_frames_bin + off * PET_FRAME_BYTES;
  lv_image_set_src(petImg, &petDsc);
}

static void pet_tick(lv_timer_t *t) {
  LV_UNUSED(t);
  if (petPoke) {
    petFrame++;
    if (petFrame >= PET_FRAME_COUNT) {
      petPoke = false;
      petFrame = 0;
    }
  } else {
    petFrame = (petFrame + 1) % PET_FRAME_COUNT;
  }
  pet_show_frame();
}

static void remainText(time_t at, char *buf, size_t n) {
  if (at <= 0) {
    buf[0] = 0;
    return;
  }
  time_t now = time(nullptr);
  long s = (long)(at - now);
  if (s < 0) s = 0;
  if (s >= 86400) {
    snprintf(buf, n, "%ldd", s / 86400);
    return;
  }
  long h = s / 3600;
  long m = (s % 3600) / 60;
  if (h > 0 && m > 0) snprintf(buf, n, "%ldh%02ldm", h, m);
  else if (h > 0) snprintf(buf, n, "%ldh", h);
  else snprintf(buf, n, "%ldm", m);
}

static void applyTheme() {
  const Palette &p = config_dark() ? DARK : LIGHT;
  lv_obj_set_style_bg_color(scr, p.bg, 0);
  styleCard(statusCard, p);
  styleCard(planCard, p);
  styleCard(petZone, p);
  lv_obj_set_style_text_color(timeLbl, p.ink, 0);
  lv_obj_set_style_bg_color(rule, config_dark() ? lv_color_hex(0x4a5362)
                                                : lv_color_hex(0xc5cbd4), 0);
  lv_obj_set_style_text_color(battLbl, p.ink2, 0);
  lv_obj_set_style_text_color(battPct, p.ink2, 0);
  lv_obj_set_style_text_color(glmTitle, p.ink, 0);
  lv_obj_set_style_text_color(curTitle, p.ink, 0);
  lv_obj_set_style_text_color(glmK1, p.ink2, 0);
  lv_obj_set_style_text_color(glmK2, p.ink2, 0);
  lv_obj_set_style_text_color(curK1, p.ink2, 0);
  lv_obj_set_style_text_color(curK2, p.ink2, 0);
  lv_obj_set_style_text_color(glmR1, p.ink2, 0);
  lv_obj_set_style_text_color(glmR2, p.ink2, 0);
  lv_obj_set_style_text_color(curReset, p.ink2, 0);
  lv_obj_set_style_text_color(glmP1, p.ink, 0);
  lv_obj_set_style_text_color(glmP2, p.ink, 0);
  lv_obj_set_style_text_color(curP1, p.ink, 0);
  lv_obj_set_style_text_color(curP2, p.ink, 0);
  lv_obj_set_style_bg_color(glmBar1, p.track, LV_PART_MAIN);
  lv_obj_set_style_bg_color(glmBar2, p.track, LV_PART_MAIN);
  lv_obj_set_style_bg_color(curBar1, p.track, LV_PART_MAIN);
  lv_obj_set_style_bg_color(curBar2, p.track, LV_PART_MAIN);
  pet_show_frame();
  ui_set_wifi(wifiOn);
}

static void onTheme(lv_event_t *e) {
  LV_UNUSED(e);
  config_set_dark(!config_dark());
  applyTheme();
}

static void onPet(lv_event_t *e) {
  LV_UNUSED(e);
  petPoke = true;
  petFrame = 0;
  pet_show_frame();
}

static void dot_set_opa(void *obj, int32_t v) {
  lv_obj_set_style_bg_opa((lv_obj_t *)obj, (lv_opa_t)v, 0);
}

static lv_obj_t *mkDot(lv_obj_t *parent) {
  lv_obj_t *d = lv_obj_create(parent);
  noScroll(d);
  lv_obj_set_size(d, 6, 6);
  lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(d, COL_OK, 0);
  lv_obj_set_style_bg_opa(d, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(d, 0, 0);
  lv_obj_add_flag(d, LV_OBJ_FLAG_HIDDEN);
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, d);
  lv_anim_set_values(&a, LV_OPA_40, LV_OPA_COVER);
  lv_anim_set_duration(&a, 1000);
  lv_anim_set_reverse_duration(&a, 1000);
  lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
  lv_anim_set_exec_cb(&a, dot_set_opa);
  lv_anim_start(&a);
  return d;
}

static lv_obj_t *mkRow(lv_obj_t *parent, lv_obj_t **k, lv_obj_t **r, lv_obj_t **pct) {
  lv_obj_t *line = lv_obj_create(parent);
  noScroll(line);
  lv_obj_set_size(line, lv_pct(100), 14);
  lv_obj_set_style_bg_opa(line, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(line, 0, 0);
  lv_obj_set_style_pad_all(line, 0, 0);
  *k = mkLbl(line, &lv_font_montserrat_12, lv_color_white());
  lv_obj_align(*k, LV_ALIGN_LEFT_MID, 0, 0);
  *pct = mkLbl(line, &lv_font_montserrat_12, lv_color_white());
  lv_obj_align(*pct, LV_ALIGN_RIGHT_MID, 0, 0);
  *r = mkLbl(line, &lv_font_montserrat_12, lv_color_white());
  lv_obj_align(*r, LV_ALIGN_RIGHT_MID, -36, 0);
  return line;
}

static lv_obj_t *mkBlock(lv_obj_t *parent) {
  lv_obj_t *b = lv_obj_create(parent);
  noScroll(b);
  lv_obj_set_width(b, lv_pct(100));
  lv_obj_set_flex_grow(b, 1);
  lv_obj_set_flex_flow(b, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(b, 3, 0);
  lv_obj_set_style_pad_all(b, 0, 0);
  lv_obj_set_style_bg_opa(b, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(b, 0, 0);
  return b;
}

static lv_obj_t *mkHead(lv_obj_t *parent) {
  lv_obj_t *head = lv_obj_create(parent);
  noScroll(head);
  lv_obj_set_size(head, lv_pct(100), 14);
  lv_obj_set_style_bg_opa(head, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(head, 0, 0);
  lv_obj_set_style_pad_all(head, 0, 0);
  return head;
}

void ui_create() {
  scr = lv_screen_active();
  lv_obj_set_style_pad_all(scr, 0, 0);
  noScroll(scr);

  statusCard = lv_obj_create(scr);
  lv_obj_set_pos(statusCard, 6, 6);
  lv_obj_set_size(statusCard, 80, 160);
  noScroll(statusCard);
  lv_obj_set_style_pad_top(statusCard, 8, 0);
  lv_obj_set_style_pad_bottom(statusCard, 8, 0);
  lv_obj_set_style_pad_hor(statusCard, 6, 0);

  timeLbl = mkLbl(statusCard, &lv_font_montserrat_16, lv_color_white());
  lv_obj_set_width(timeLbl, lv_pct(100));
  lv_obj_set_style_text_align(timeLbl, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(timeLbl, "--:--");

  rule = lv_obj_create(statusCard);
  lv_obj_set_size(rule, 52, 1);
  lv_obj_align(rule, LV_ALIGN_TOP_MID, 0, 28);
  lv_obj_set_style_border_width(rule, 0, 0);
  lv_obj_set_style_pad_all(rule, 0, 0);
  lv_obj_set_style_bg_opa(rule, LV_OPA_COVER, 0);

  lv_obj_t *statusFoot = lv_obj_create(statusCard);
  noScroll(statusFoot);
  lv_obj_set_size(statusFoot, lv_pct(100), 16);
  lv_obj_align(statusFoot, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_flex_flow(statusFoot, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(statusFoot, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_all(statusFoot, 0, 0);
  lv_obj_set_style_pad_column(statusFoot, 3, 0);
  lv_obj_set_style_bg_opa(statusFoot, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(statusFoot, 0, 0);
  wifiLbl = mkLbl(statusFoot, &lv_font_montserrat_12, lv_color_white());
  lv_label_set_text(wifiLbl, LV_SYMBOL_WIFI);
  battLbl = mkLbl(statusFoot, &lv_font_montserrat_12, lv_color_white());
  lv_label_set_text(battLbl, LV_SYMBOL_BATTERY_FULL);
  battPct = mkLbl(statusFoot, &lv_font_montserrat_12, lv_color_white());
  lv_label_set_text(battPct, "--");

  lv_obj_t *themeHit = lv_obj_create(statusCard);
  noScroll(themeHit);
  lv_obj_set_size(themeHit, lv_pct(100), 72);
  lv_obj_align(themeHit, LV_ALIGN_TOP_MID, 0, 34);
  lv_obj_set_style_bg_opa(themeHit, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(themeHit, 0, 0);
  lv_obj_set_style_pad_all(themeHit, 0, 0);
  lv_obj_add_flag(themeHit, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(themeHit, onTheme, LV_EVENT_CLICKED, nullptr);

  planCard = lv_obj_create(scr);
  lv_obj_set_pos(planCard, 94, 6);
  lv_obj_set_size(planCard, 372, 160);
  noScroll(planCard);
  lv_obj_set_style_pad_top(planCard, 5, 0);
  lv_obj_set_style_pad_bottom(planCard, 5, 0);
  lv_obj_set_style_pad_hor(planCard, 10, 0);
  lv_obj_set_flex_flow(planCard, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(planCard, 4, 0);

  lv_obj_t *glmBlock = mkBlock(planCard);
  lv_obj_t *glmHead = mkHead(glmBlock);
  glmTitle = mkLbl(glmHead, &lv_font_montserrat_12, lv_color_white());
  lv_label_set_text(glmTitle, "GLM");
  glmDot = mkDot(glmHead);
  lv_obj_align(glmDot, LV_ALIGN_LEFT_MID, 40, 0);

  mkRow(glmBlock, &glmK1, &glmR1, &glmP1);
  lv_label_set_text(glmK1, "5h");
  glmBar1 = mkBar(glmBlock, DARK);
  mkRow(glmBlock, &glmK2, &glmR2, &glmP2);
  lv_label_set_text(glmK2, "7d");
  glmBar2 = mkBar(glmBlock, DARK);

  lv_obj_t *curBlock = mkBlock(planCard);
  lv_obj_t *curHead = mkHead(curBlock);
  curTitle = mkLbl(curHead, &lv_font_montserrat_12, lv_color_white());
  lv_label_set_text(curTitle, "CURSOR");
  curDot = mkDot(curHead);
  lv_obj_align(curDot, LV_ALIGN_LEFT_MID, 62, 0);
  curReset = mkLbl(curHead, &lv_font_montserrat_12, lv_color_white());
  lv_obj_align(curReset, LV_ALIGN_RIGHT_MID, 0, 0);

  lv_obj_t *curResetDummy;
  mkRow(curBlock, &curK1, &curResetDummy, &curP1);
  lv_label_set_text(curK1, "composer & grok");
  lv_label_set_text(curResetDummy, "");
  curBar1 = mkBar(curBlock, DARK);
  lv_obj_t *curResetDummy2;
  mkRow(curBlock, &curK2, &curResetDummy2, &curP2);
  lv_label_set_text(curK2, "other");
  curBar2 = mkBar(curBlock, DARK);

  petZone = lv_obj_create(scr);
  lv_obj_set_pos(petZone, 474, 6);
  lv_obj_set_size(petZone, 160, 160);
  noScroll(petZone);
  lv_obj_set_style_pad_all(petZone, 0, 0);
  lv_obj_add_event_cb(petZone, onPet, LV_EVENT_CLICKED, nullptr);
  lv_obj_add_flag(petZone, LV_OBJ_FLAG_CLICKABLE);
  petImg = lv_image_create(petZone);
  noScroll(petImg);
  lv_obj_center(petImg);
  lv_obj_clear_flag(petImg, LV_OBJ_FLAG_CLICKABLE);
  petTimer = lv_timer_create(pet_tick, 110, nullptr);

  applyTheme();
}

static void setBar(lv_obj_t *bar, lv_obj_t *pctLbl, lv_obj_t *resetLbl, const QuotaBar &q, bool fail) {
  if (fail) {
    lv_label_set_text(pctLbl, "fail");
    if (resetLbl) lv_label_set_text(resetLbl, "");
    lv_bar_set_value(bar, 0, LV_ANIM_OFF);
    return;
  }
  if (!q.present) {
    lv_label_set_text(pctLbl, "--");
    if (resetLbl) lv_label_set_text(resetLbl, "");
    lv_bar_set_value(bar, 0, LV_ANIM_OFF);
    return;
  }
  char pbuf[8];
  snprintf(pbuf, sizeof(pbuf), "%d%%", q.pct);
  lv_label_set_text(pctLbl, pbuf);
  lv_bar_set_value(bar, q.pct, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(bar, tone(q.pct), LV_PART_INDICATOR);
  if (resetLbl) {
    char rbuf[12];
    remainText(q.resetAt, rbuf, sizeof(rbuf));
    lv_label_set_text(resetLbl, rbuf);
  }
}

void ui_apply(const Snapshot &s) {
  setBar(glmBar1, glmP1, glmR1, s.h5, !s.glmOk);
  setBar(glmBar2, glmP2, glmR2, s.week, !s.glmOk);
  if (!s.glmOk) lv_obj_add_flag(glmBar2, LV_OBJ_FLAG_HIDDEN);
  else if (s.week.present) lv_obj_clear_flag(glmBar2, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(glmBar2, LV_OBJ_FLAG_HIDDEN);

  setBar(curBar1, curP1, nullptr, s.autoBar, !s.cursorOk);
  setBar(curBar2, curP2, nullptr, s.apiBar, !s.cursorOk);
  char cyc[12];
  remainText(s.cycleEnd, cyc, sizeof(cyc));
  lv_label_set_text(curReset, s.cursorOk ? cyc : "");

  if (s.glmDot) lv_obj_clear_flag(glmDot, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(glmDot, LV_OBJ_FLAG_HIDDEN);
  if (s.curDot) lv_obj_clear_flag(curDot, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(curDot, LV_OBJ_FLAG_HIDDEN);

  if (s.petState.length() && s.petState != petFollow) {
    strlcpy(petFollow, s.petState.c_str(), sizeof(petFollow));
    if (!petOverride[0] && !petPoke) {
      petFrame = 0;
      pet_show_frame();
    }
  }
}

void ui_set_pet_override(const char *state) {
  if (!state || !state[0] || strcmp(state, "auto") == 0) {
    petOverride[0] = 0;
  } else if (pet_index_of(state) == 0 && strcmp(state, "idle") != 0) {
    Serial.println("unknown pet state");
    return;
  } else {
    strlcpy(petOverride, state, sizeof(petOverride));
  }
  petPoke = false;
  petFrame = 0;
  if (lvgl_port_lock(50)) {
    pet_show_frame();
    lvgl_port_unlock();
  }
}

void ui_set_wifi(bool up) {
  wifiOn = up;
  if (up) {
    const Palette &p = config_dark() ? DARK : LIGHT;
    lv_obj_set_style_text_color(wifiLbl, p.ink2, 0);
  } else {
    lv_obj_set_style_text_color(wifiLbl, COL_BAD, 0);
  }
  lv_obj_set_style_text_opa(wifiLbl, LV_OPA_COVER, 0);
}

void ui_set_power(bool charging, int pct) {
  if (pct < 0 || charging) {
    lv_label_set_text(battLbl, LV_SYMBOL_CHARGE);
  } else if (pct >= 90) {
    lv_label_set_text(battLbl, LV_SYMBOL_BATTERY_FULL);
  } else if (pct >= 70) {
    lv_label_set_text(battLbl, LV_SYMBOL_BATTERY_3);
  } else if (pct >= 45) {
    lv_label_set_text(battLbl, LV_SYMBOL_BATTERY_2);
  } else if (pct >= 20) {
    lv_label_set_text(battLbl, LV_SYMBOL_BATTERY_1);
  } else {
    lv_label_set_text(battLbl, LV_SYMBOL_BATTERY_EMPTY);
  }
  if (pct < 0) {
    lv_label_set_text(battPct, "--");
    return;
  }
  char buf[8];
  snprintf(buf, sizeof(buf), "%d%%", pct);
  lv_label_set_text(battPct, buf);
}

void ui_tick_clock() {
  time_t now = time(nullptr);
  struct tm t;
  if (now < 100000 || !localtime_r(&now, &t)) {
    lv_label_set_text(timeLbl, "--:--");
    return;
  }
  char buf[8];
  strftime(buf, sizeof(buf), "%H:%M", &t);
  lv_label_set_text(timeLbl, buf);
}
