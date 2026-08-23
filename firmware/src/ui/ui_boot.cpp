#include "ui_boot.h"
#include "ui_internal.h"
#include "pet_frames.h"
#include "config.h"
#include "net.h"
#include "lvgl.h"
#include <Arduino.h>

static const uint32_t BOOT_MAX_MS = 10000;    /* wait for first data at most this long */
static const uint32_t BOOT_NO_WIFI_MS = 2500;  /* unconfigured: hand over to the hint quickly */
static const uint32_t FRAME_MS = 110;
static const int LOOPS_PER_STATE = 2;          /* ~1.3s per pet state */

static lv_obj_t *bootScr;
static lv_obj_t *bootImg;
static lv_image_dsc_t bootDsc;
static lv_timer_t *bootTimer;
static int bootFrame, bootState, loopsDone;
static uint32_t bootStart;
static bool bootActive = false;

static void show_frame() {
  const int theme = config_dark() ? 0 : 1;
  const uint32_t off =
      ((uint32_t)theme * PET_STATE_COUNT + bootState) * PET_FRAME_COUNT +
      bootFrame;
  memset(&bootDsc, 0, sizeof(bootDsc));
  bootDsc.header.magic = LV_IMAGE_HEADER_MAGIC;
  bootDsc.header.cf = LV_COLOR_FORMAT_RGB565;
  bootDsc.header.w = PET_W;
  bootDsc.header.h = PET_H;
  bootDsc.header.stride = PET_W * 2;
  bootDsc.data_size = PET_FRAME_BYTES;
  bootDsc.data = pet_frames_bin + off * PET_FRAME_BYTES;
  lv_image_set_src(bootImg, &bootDsc);
}

static void tick(lv_timer_t *t) {
  LV_UNUSED(t);
  bootFrame++;
  if (bootFrame >= PET_FRAME_COUNT) {
    bootFrame = 0;
    loopsDone++;
  }
  if (loopsDone >= LOOPS_PER_STATE) {
    loopsDone = 0;
    bootState = (bootState + 1) % PET_STATE_COUNT;
  }
  show_frame();
}

static void end_boot(const char *why) {
  if (!bootActive) return;
  bootActive = false;
  lv_timer_pause(bootTimer);
  lv_screen_load_anim(uiScr, LV_SCR_LOAD_ANIM_FADE_ON, 300, 0, false);
  ui_pet_set_paused(false);
  Serial.printf("[boot] exit %s\n", why);
}

static void onSkip(lv_event_t *e) {
  LV_UNUSED(e);
  end_boot("skip");
}

void ui_boot_begin() {
  if (!bootScr) {
    bootScr = lv_obj_create(nullptr);
    lv_obj_set_style_pad_all(bootScr, 0, 0);
    ui_no_scroll(bootScr);
    bootImg = lv_image_create(bootScr);
    ui_no_scroll(bootImg);
    lv_obj_clear_flag(bootImg, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(bootImg, 240, 6); /* 160px sprite centered on 640x172 */
    bootTimer = lv_timer_create(tick, FRAME_MS, nullptr);
    lv_obj_add_flag(bootScr, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(bootScr, onSkip, LV_EVENT_CLICKED, nullptr);
  }
  lv_obj_set_style_bg_color(bootScr, ui_palette().bg, 0);
  bootFrame = 0;
  bootState = 0;
  loopsDone = 0;
  bootStart = millis();
  bootActive = true;
  lv_timer_resume(bootTimer);
  show_frame();
  ui_pet_set_paused(true);
  lv_screen_load(bootScr);
  Serial.println("[boot] splash");
}

void ui_boot_poll() {
  if (!bootActive) return;
  const uint32_t limit =
      config_wifi_ssid().isEmpty() ? BOOT_NO_WIFI_MS : BOOT_MAX_MS;
  if (net_snapshot().fresh) end_boot("data");
  else if (millis() - bootStart > limit) end_boot("timeout");
}

bool ui_boot_active() { return bootActive; }
