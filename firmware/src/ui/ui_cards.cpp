#include "ui_internal.h"
#include "config.h"

void ui_no_scroll(lv_obj_t *o) {
  lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(o, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_style_outline_width(o, 0, 0);
  lv_obj_set_style_min_height(o, 0, 0);
}

void ui_style_card(lv_obj_t *o) {
  const UiPalette &p = ui_palette();
  ui_no_scroll(o);
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

lv_obj_t *ui_mk_lbl(lv_obj_t *parent, const lv_font_t *font, lv_color_t c) {
  lv_obj_t *l = lv_label_create(parent);
  ui_no_scroll(l);
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_color(l, c, 0);
  lv_label_set_long_mode(l, LV_LABEL_LONG_CLIP);
  lv_label_set_text(l, "");
  return l;
}

lv_obj_t *ui_mk_bar(lv_obj_t *parent) {
  lv_obj_t *b = lv_bar_create(parent);
  ui_no_scroll(b);
  lv_obj_set_size(b, lv_pct(100), 7);
  lv_bar_set_range(b, 0, 100);
  lv_obj_set_style_radius(b, 3, 0);
  lv_obj_set_style_bg_color(b, ui_palette().track, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(b, 3, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(b, UI_OK, LV_PART_INDICATOR);
  return b;
}

static void dot_set_opa(void *obj, int32_t v) {
  lv_obj_set_style_bg_opa((lv_obj_t *)obj, (lv_opa_t)v, 0);
}

lv_obj_t *ui_mk_dot(lv_obj_t *parent) {
  lv_obj_t *d = lv_obj_create(parent);
  ui_no_scroll(d);
  lv_obj_set_size(d, 6, 6);
  lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(d, UI_OK, 0);
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

lv_obj_t *ui_mk_row(lv_obj_t *parent, lv_obj_t **k, lv_obj_t **r, lv_obj_t **pct) {
  lv_obj_t *line = lv_obj_create(parent);
  ui_no_scroll(line);
  lv_obj_set_size(line, lv_pct(100), 14);
  lv_obj_set_style_bg_opa(line, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(line, 0, 0);
  lv_obj_set_style_pad_all(line, 0, 0);
  *k = ui_mk_lbl(line, &lv_font_montserrat_12, lv_color_white());
  lv_obj_align(*k, LV_ALIGN_LEFT_MID, 0, 0);
  *pct = ui_mk_lbl(line, &lv_font_montserrat_12, lv_color_white());
  lv_obj_align(*pct, LV_ALIGN_RIGHT_MID, 0, 0);
  *r = ui_mk_lbl(line, &lv_font_montserrat_12, lv_color_white());
  lv_obj_align(*r, LV_ALIGN_RIGHT_MID, -36, 0);
  return line;
}

lv_obj_t *ui_mk_block(lv_obj_t *parent) {
  lv_obj_t *b = lv_obj_create(parent);
  ui_no_scroll(b);
  lv_obj_set_width(b, lv_pct(100));
  lv_obj_set_flex_grow(b, 1);
  lv_obj_set_flex_flow(b, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(b, 3, 0);
  lv_obj_set_style_pad_all(b, 0, 0);
  lv_obj_set_style_bg_opa(b, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(b, 0, 0);
  return b;
}

lv_obj_t *ui_mk_head(lv_obj_t *parent) {
  lv_obj_t *head = lv_obj_create(parent);
  ui_no_scroll(head);
  lv_obj_set_size(head, lv_pct(100), 14);
  lv_obj_set_style_bg_opa(head, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(head, 0, 0);
  lv_obj_set_style_pad_all(head, 0, 0);
  return head;
}
