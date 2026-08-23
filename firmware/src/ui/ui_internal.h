#pragma once

#include "lvgl.h"
#include "ui_theme.h"

/* Dashboard widget pointers, defined in ui.cpp. */
extern lv_obj_t *uiScr;
extern lv_obj_t *statusCard, *planCard, *petZone;
extern lv_obj_t *timeLbl, *rule, *wifiLbl, *battLbl, *battPct;
extern lv_obj_t *glmTitle, *glmDot, *glmK1, *glmR1, *glmP1, *glmBar1;
extern lv_obj_t *glmK2, *glmR2, *glmP2, *glmBar2;
extern lv_obj_t *curTitle, *curDot, *curReset;
extern lv_obj_t *curK1, *curP1, *curBar1, *curK2, *curP2, *curBar2;

/* Widget builders (ui_cards.cpp). */
void ui_no_scroll(lv_obj_t *o);
void ui_style_card(lv_obj_t *o);
lv_obj_t *ui_mk_lbl(lv_obj_t *parent, const lv_font_t *font, lv_color_t c);
lv_obj_t *ui_mk_bar(lv_obj_t *parent);
lv_obj_t *ui_mk_dot(lv_obj_t *parent);
lv_obj_t *ui_mk_row(lv_obj_t *parent, lv_obj_t **k, lv_obj_t **r, lv_obj_t **pct);
lv_obj_t *ui_mk_block(lv_obj_t *parent);
lv_obj_t *ui_mk_head(lv_obj_t *parent);

/* Pet renderer (ui_pet.cpp). */
extern lv_obj_t *petImg;
void ui_pet_create(lv_obj_t *parent);
void ui_pet_poke();
void ui_pet_show_frame();
int ui_pet_index_of(const char *name);
const char *ui_pet_active_name();
bool ui_pet_set_follow(const char *state);
bool ui_pet_set_override(const char *state);
void ui_pet_set_paused(bool paused);
