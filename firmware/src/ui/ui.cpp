#include "ui.h"
#include "ui_internal.h"
#include "config.h"
#include "power.h"
#include "standby.h"
#include "ui/ui_settings.h"
#include "bsp/lvgl_port.h"
#include "lvgl.h"
#include <Arduino.h>
#include <time.h>

lv_obj_t *uiScr;
lv_obj_t *statusCard, *planCard, *petZone;
lv_obj_t *timeLbl, *rule, *wifiLbl, *battLbl, *battPct;
lv_obj_t *glmTitle, *glmDot, *glmK1, *glmR1, *glmP1, *glmBar1;
lv_obj_t *glmK2, *glmR2, *glmP2, *glmBar2;
static lv_obj_t *glmPeak; /* 「• 3x」peak-pricing badge, shown after GLM */
lv_obj_t *curTitle, *curDot, *curReset;
lv_obj_t *curK1, *curP1, *curBar1, *curK2, *curP2, *curBar2;
static lv_obj_t *ovlCap, *ovlBig, *ovlRing;
static lv_obj_t *setupHint;
static bool hintShown;

static bool wifiOn = true;
static bool srvOn = true;
static bool uiAsleep = false;
static lv_obj_t *sleepScr;
static bool glmPeakOn = false;

/* Peak-pricing badge "• 3x": bullet takes the theme ink via the label style,
 * only "3x" is hard-colored to UI_WARN (#f0805a) through label recolor. */
static void drawGlmPeak() {
  lv_label_set_text(glmPeak, "\xE2\x80\xA2 #f0805a 3x#");
  lv_obj_clear_flag(glmPeak, LV_OBJ_FLAG_HIDDEN);
  lv_obj_update_layout(glmTitle);
  lv_coord_t x = lv_obj_get_width(glmTitle) + 4;
  lv_obj_align(glmPeak, LV_ALIGN_LEFT_MID, x, 0);
  lv_obj_update_layout(glmPeak);
  lv_obj_align(glmDot, LV_ALIGN_LEFT_MID, x + lv_obj_get_width(glmPeak) + 5, 0);
}

void ui_set_glm_peak(bool on) {
  if (on == glmPeakOn) return;
  glmPeakOn = on;
  if (on) {
    drawGlmPeak();
    return;
  }
  lv_obj_add_flag(glmPeak, LV_OBJ_FLAG_HIDDEN);
  lv_obj_align(glmDot, LV_ALIGN_LEFT_MID, 40, 0); /* back to the idle slot */
}

static void applyTheme() {
  const UiPalette &p = ui_palette();
  lv_obj_set_style_bg_color(uiScr, p.bg, 0);
  ui_style_card(statusCard);
  ui_style_card(planCard);
  ui_style_card(petZone);
  lv_obj_set_style_text_color(timeLbl, p.ink, 0);
  lv_obj_set_style_bg_color(rule, config_dark() ? lv_color_hex(0x4a5362)
                                                : lv_color_hex(0xc5cbd4), 0);
  lv_obj_set_style_text_color(battLbl, p.ink2, 0);
  lv_obj_set_style_text_color(battPct, p.ink2, 0);
  lv_obj_set_style_text_color(glmTitle, p.ink, 0);
  lv_obj_set_style_text_color(glmPeak, p.ink, 0);
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
  lv_obj_set_style_text_color(ovlCap, p.ink2, 0);
  lv_obj_set_style_arc_color(ovlRing, p.track, LV_PART_MAIN);
  lv_obj_set_style_text_color(setupHint, p.ink2, 0);
  if (glmPeakOn) drawGlmPeak(); /* theme flip: rebuild recolor positions/colors */
  ui_pet_show_frame();
  ui_set_link(wifiOn, srvOn);
}

static void onTheme(lv_event_t *e) {
  LV_UNUSED(e);
  if (power_phase() != POWER_NORMAL) return; /* key overlay is showing */
  config_set_dark(!config_dark());
  applyTheme();
}

static void onPet(lv_event_t *e) {
  LV_UNUSED(e);
  ui_pet_poke();
}

static void text_set_opa(void *obj, int32_t v) {
  lv_obj_set_style_text_opa((lv_obj_t *)obj, (lv_opa_t)v, 0);
}

static void start_hint_blink() {
  lv_anim_t blink;
  lv_anim_init(&blink);
  lv_anim_set_var(&blink, setupHint);
  lv_anim_set_values(&blink, LV_OPA_30, LV_OPA_COVER);
  lv_anim_set_duration(&blink, 700);
  lv_anim_set_reverse_duration(&blink, 700);
  lv_anim_set_repeat_count(&blink, LV_ANIM_REPEAT_INFINITE);
  lv_anim_set_exec_cb(&blink, text_set_opa);
  lv_anim_start(&blink);
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

void ui_create() {
  uiScr = lv_screen_active();
  lv_obj_set_style_pad_all(uiScr, 0, 0);
  ui_no_scroll(uiScr);

  statusCard = lv_obj_create(uiScr);
  lv_obj_set_pos(statusCard, 6, 6);
  lv_obj_set_size(statusCard, 80, 160);
  ui_no_scroll(statusCard);
  lv_obj_set_style_pad_top(statusCard, 8, 0);
  lv_obj_set_style_pad_bottom(statusCard, 8, 0);
  lv_obj_set_style_pad_hor(statusCard, 6, 0);

  timeLbl = ui_mk_lbl(statusCard, &lv_font_montserrat_16, lv_color_white());
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
  ui_no_scroll(statusFoot);
  lv_obj_set_size(statusFoot, lv_pct(100), 16);
  lv_obj_align(statusFoot, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_flex_flow(statusFoot, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(statusFoot, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_all(statusFoot, 0, 0);
  lv_obj_set_style_pad_column(statusFoot, 3, 0);
  lv_obj_set_style_bg_opa(statusFoot, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(statusFoot, 0, 0);
  wifiLbl = ui_mk_lbl(statusFoot, &lv_font_montserrat_12, lv_color_white());
  lv_label_set_text(wifiLbl, LV_SYMBOL_WIFI);
  battLbl = ui_mk_lbl(statusFoot, &lv_font_montserrat_12, lv_color_white());
  lv_label_set_text(battLbl, LV_SYMBOL_BATTERY_FULL);
  battPct = ui_mk_lbl(statusFoot, &lv_font_montserrat_12, lv_color_white());
  lv_label_set_text(battPct, "--");

  lv_obj_t *themeHit = lv_obj_create(statusCard);
  ui_no_scroll(themeHit);
  lv_obj_set_size(themeHit, lv_pct(100), 72);
  lv_obj_align(themeHit, LV_ALIGN_TOP_MID, 0, 34);
  lv_obj_set_style_bg_opa(themeHit, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(themeHit, 0, 0);
  lv_obj_set_style_pad_all(themeHit, 0, 0);
  lv_obj_add_flag(themeHit, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(themeHit, onTheme, LV_EVENT_CLICKED, nullptr);

  /* Key-feedback overlay in the reserved mid area of the status card:
   * caption ("OFF"/"REBOOT"), a big digit, and a circular hold-progress ring. */
  ovlCap = ui_mk_lbl(statusCard, &lv_font_montserrat_12, lv_color_white());
  lv_obj_set_width(ovlCap, lv_pct(100));
  lv_obj_set_style_text_align(ovlCap, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(ovlCap, LV_ALIGN_TOP_MID, 0, 38);
  ovlBig = ui_mk_lbl(statusCard, &lv_font_montserrat_16, lv_color_white());
  lv_obj_set_width(ovlBig, lv_pct(100));
  lv_obj_set_style_text_align(ovlBig, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(ovlBig, LV_ALIGN_TOP_MID, 0, 54);
  ovlRing = lv_arc_create(statusCard);
  ui_no_scroll(ovlRing);
  lv_obj_clear_flag(ovlRing, LV_OBJ_FLAG_CLICKABLE); /* touch must not scrub it */
  lv_obj_set_size(ovlRing, 46, 46);
  lv_obj_align(ovlRing, LV_ALIGN_TOP_MID, 0, 58);
  lv_arc_set_rotation(ovlRing, 270); /* start at 12 o'clock */
  lv_arc_set_bg_angles(ovlRing, 0, 360);
  lv_arc_set_range(ovlRing, 0, 1000);
  lv_arc_set_value(ovlRing, 0);
  lv_obj_set_style_arc_width(ovlRing, 4, LV_PART_MAIN);
  lv_obj_set_style_arc_width(ovlRing, 4, LV_PART_INDICATOR);
  lv_obj_set_style_arc_rounded(ovlRing, true, LV_PART_MAIN);
  lv_obj_set_style_arc_rounded(ovlRing, true, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(ovlRing, UI_WARN, LV_PART_INDICATOR);
  lv_obj_remove_style(ovlRing, nullptr, LV_PART_KNOB); /* no knob dot */
  lv_obj_add_flag(ovlCap, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ovlBig, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ovlRing, LV_OBJ_FLAG_HIDDEN);

  /* First-boot hint until WiFi gets configured (BOOT double-click = setup). */
  setupHint = ui_mk_lbl(statusCard, &lv_font_montserrat_12, lv_color_white());
  lv_obj_set_width(setupHint, lv_pct(100));
  lv_obj_set_style_text_align(setupHint, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(setupHint, "Setup:\nBOOT x2");
  lv_obj_align(setupHint, LV_ALIGN_TOP_MID, 0, 56);
  lv_obj_add_flag(setupHint, LV_OBJ_FLAG_HIDDEN);
  start_hint_blink();

  planCard = lv_obj_create(uiScr);
  lv_obj_set_pos(planCard, 94, 6);
  lv_obj_set_size(planCard, 372, 160);
  ui_no_scroll(planCard);
  lv_obj_set_style_pad_top(planCard, 5, 0);
  lv_obj_set_style_pad_bottom(planCard, 5, 0);
  lv_obj_set_style_pad_hor(planCard, 10, 0);
  lv_obj_set_flex_flow(planCard, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(planCard, 4, 0);

  lv_obj_t *glmBlock = ui_mk_block(planCard);
  lv_obj_t *glmHead = ui_mk_head(glmBlock);
  glmTitle = ui_mk_lbl(glmHead, &lv_font_montserrat_12, lv_color_white());
  lv_label_set_text(glmTitle, "GLM");
  glmDot = ui_mk_dot(glmHead);
  lv_obj_align(glmDot, LV_ALIGN_LEFT_MID, 40, 0); /* "GLM" idle slot; moves right while the peak badge shows */
  glmPeak = ui_mk_lbl(glmHead, &lv_font_montserrat_12, lv_color_white());
  lv_label_set_recolor(glmPeak, true); /* text re-colored per theme in drawGlmPeak() */

  ui_mk_row(glmBlock, &glmK1, &glmR1, &glmP1);
  lv_label_set_text(glmK1, "5h");
  glmBar1 = ui_mk_bar(glmBlock);
  ui_mk_row(glmBlock, &glmK2, &glmR2, &glmP2);
  lv_label_set_text(glmK2, "7d");
  glmBar2 = ui_mk_bar(glmBlock);

  lv_obj_t *curBlock = ui_mk_block(planCard);
  lv_obj_t *curHead = ui_mk_head(curBlock);
  curTitle = ui_mk_lbl(curHead, &lv_font_montserrat_12, lv_color_white());
  lv_label_set_text(curTitle, "CURSOR");
  curDot = ui_mk_dot(curHead);
  lv_obj_align(curDot, LV_ALIGN_LEFT_MID, 62, 0);
  curReset = ui_mk_lbl(curHead, &lv_font_montserrat_12, lv_color_white());
  lv_obj_align(curReset, LV_ALIGN_RIGHT_MID, 0, 0);

  lv_obj_t *curResetDummy;
  ui_mk_row(curBlock, &curK1, &curResetDummy, &curP1);
  lv_label_set_text(curK1, "composer & grok");
  lv_label_set_text(curResetDummy, "");
  curBar1 = ui_mk_bar(curBlock);
  lv_obj_t *curResetDummy2;
  ui_mk_row(curBlock, &curK2, &curResetDummy2, &curP2);
  lv_label_set_text(curK2, "other");
  curBar2 = ui_mk_bar(curBlock);

  petZone = lv_obj_create(uiScr);
  lv_obj_set_pos(petZone, 474, 6);
  lv_obj_set_size(petZone, 160, 160);
  ui_no_scroll(petZone);
  lv_obj_set_style_pad_all(petZone, 0, 0);
  lv_obj_add_event_cb(petZone, onPet, LV_EVENT_CLICKED, nullptr);
  lv_obj_add_flag(petZone, LV_OBJ_FLAG_CLICKABLE);
  ui_pet_create(petZone);

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
  lv_obj_set_style_bg_color(bar, ui_tone(q.pct), LV_PART_INDICATOR);
  if (resetLbl) {
    char rbuf[12];
    remainText(q.resetAt, rbuf, sizeof(rbuf));
    lv_label_set_text(resetLbl, rbuf);
  }
}

void ui_apply(const Snapshot &s) {
  if (uiAsleep) return; /* no redraws while the backlight is off */
  ui_set_glm_peak(s.glmPeak);
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

  if (s.petState.length()) ui_pet_set_follow(s.petState.c_str());
}

void ui_set_pet_override(const char *state) {
  if (!ui_pet_set_override(state)) {
    Serial.println("unknown pet state");
    return;
  }
  if (lvgl_port_lock(50)) {
    ui_pet_show_frame();
    lvgl_port_unlock();
  }
}

void ui_set_link(bool up, bool srv) {
  wifiOn = up;
  srvOn = srv;
  static int lastState = -1; /* 0 ok, 1 server lost, 2 wifi down */
  int st = !up ? 2 : (!srv ? 1 : 0);
  if (st != lastState) {
    lastState = st;
    Serial.printf("[link] %s\n", st == 2 ? "wifi down" : st == 1 ? "server lost" : "ok");
  }
  if (uiAsleep) return;
  if (!up) {
    lv_obj_set_style_text_color(wifiLbl, UI_BAD, 0);
  } else if (!srv) {
    lv_obj_set_style_text_color(wifiLbl, UI_WARN, 0);
  } else {
    lv_obj_set_style_text_color(wifiLbl, ui_palette().ink2, 0);
  }
  lv_obj_set_style_text_opa(wifiLbl, LV_OPA_COVER, 0);
}

static void onSleepTouch(lv_event_t *e) {
  LV_UNUSED(e);
  standby_request_wake();
}

void ui_set_sleep(bool on) {
  if (on == uiAsleep) return;
  uiAsleep = on;
  if (on) {
    if (!sleepScr) {
      sleepScr = lv_obj_create(nullptr);
      lv_obj_set_style_pad_all(sleepScr, 0, 0);
      ui_no_scroll(sleepScr);
      lv_obj_set_style_bg_color(sleepScr, lv_color_black(), 0);
      lv_obj_add_flag(sleepScr, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_add_event_cb(sleepScr, onSleepTouch, LV_EVENT_PRESSED, nullptr);
    }
    /* uiScr stays the "previous" screen, so its loops must stop explicitly. */
    lv_anim_delete(setupHint, nullptr);
    ui_dots_set_paused(true);
    ui_pet_set_paused(true);
    lv_screen_load(sleepScr);
  } else {
    ui_pet_set_paused(false);
    lv_screen_load_anim(uiScr, LV_SCR_LOAD_ANIM_FADE_ON, 300, 0, false);
    start_hint_blink();
    ui_dots_set_paused(false);
  }
}

void ui_set_power(bool charging, int pct) {
  if (uiAsleep) return;
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
  if (uiAsleep) return;
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

void ui_toggle_theme() {
  if (power_phase() != POWER_NORMAL) return;
  config_set_dark(!config_dark());
  applyTheme();
  if (ui_settings_active()) ui_settings_refresh_theme();
}

void ui_poll_power() {
  static PowerPhase lastPhase = POWER_NORMAL;
  PowerPhase phase = power_phase();
  if (phase != lastPhase) {
    if (phase == POWER_NORMAL) {
      lv_obj_add_flag(ovlCap, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(ovlBig, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(ovlRing, LV_OBJ_FLAG_HIDDEN);
    }
    lastPhase = phase;
  }

  bool needHint = config_wifi_ssid().isEmpty() && phase == POWER_NORMAL;
  if (needHint != hintShown) {
    hintShown = needHint;
    if (needHint) lv_obj_clear_flag(setupHint, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(setupHint, LV_OBJ_FLAG_HIDDEN);
  }

  if (phase == POWER_NORMAL) return;

  char buf[8];
  if (phase == POWER_HOLD_OFF) {
    lv_obj_clear_flag(ovlCap, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ovlRing, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ovlBig, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(ovlCap, "OFF");
    lv_arc_set_value(ovlRing, power_hold_permille());
  } else if (phase == POWER_COUNTDOWN) {
    lv_obj_clear_flag(ovlCap, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ovlBig, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ovlRing, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(ovlCap, "OFF");
    snprintf(buf, sizeof(buf), "%u", (unsigned)power_countdown_remain());
    lv_label_set_text(ovlBig, buf);
    lv_obj_set_style_text_color(ovlBig, UI_BAD, 0);
  } else { /* POWER_HOLD_REBOOT */
    lv_obj_clear_flag(ovlCap, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ovlBig, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ovlRing, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(ovlCap, "REBOOT");
    snprintf(buf, sizeof(buf), "%u", (unsigned)power_reboot_remain());
    lv_label_set_text(ovlBig, buf);
    lv_obj_set_style_text_color(ovlBig, UI_WARN, 0);
  }
}
