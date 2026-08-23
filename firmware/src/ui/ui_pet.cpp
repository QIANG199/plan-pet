#include "ui_internal.h"
#include "pet_frames.h"
#include "config.h"
#include <string.h>

static const char *const PET_STATES[] = {
    "idle", "thinking", "typing", "happy", "error", "sleeping", "poke"};

lv_obj_t *petImg;
static lv_image_dsc_t petDsc;
static lv_timer_t *petTimer;
static int petFrame;
static int petStateIdx;
static char petFollow[16] = "idle";
static char petOverride[16];
static bool petPoke;

int ui_pet_index_of(const char *name) {
  for (int i = 0; i < PET_STATE_COUNT; i++) {
    if (strcmp(name, PET_STATES[i]) == 0) return i;
  }
  return 0;
}

const char *ui_pet_active_name() {
  return petOverride[0] ? petOverride : petFollow;
}

void ui_pet_show_frame() {
  if (!petImg) return;
  petStateIdx = petPoke ? PET_POKE_IDX : ui_pet_index_of(ui_pet_active_name());
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
  ui_pet_show_frame();
}

void ui_pet_create(lv_obj_t *parent) {
  petImg = lv_image_create(parent);
  ui_no_scroll(petImg);
  lv_obj_center(petImg);
  lv_obj_clear_flag(petImg, LV_OBJ_FLAG_CLICKABLE);
  petTimer = lv_timer_create(pet_tick, 110, nullptr);
}

void ui_pet_poke() {
  petPoke = true;
  petFrame = 0;
  ui_pet_show_frame();
}

bool ui_pet_set_follow(const char *state) {
  if (!state || !state[0] || strcmp(state, petFollow) == 0) return false;
  strlcpy(petFollow, state, sizeof(petFollow));
  if (!petOverride[0] && !petPoke) {
    petFrame = 0;
    ui_pet_show_frame();
  }
  return true;
}

bool ui_pet_set_override(const char *state) {
  bool ok = true;
  if (!state || !state[0] || strcmp(state, "auto") == 0) {
    petOverride[0] = 0;
  } else if (ui_pet_index_of(state) == 0 && strcmp(state, "idle") != 0) {
    ok = false; /* unknown state name */
  } else {
    strlcpy(petOverride, state, sizeof(petOverride));
  }
  if (ok) {
    petPoke = false;
    petFrame = 0;
  }
  return ok;
}

void ui_pet_set_paused(bool paused) {
  if (!petTimer) return;
  if (paused) lv_timer_pause(petTimer);
  else lv_timer_resume(petTimer);
}
