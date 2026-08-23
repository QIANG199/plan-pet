#include "ui_settings.h"
#include "ui_internal.h"
#include "config.h"
#include "net.h"
#include "bsp/lvgl_port.h"
#include "lvgl.h"
#include <Arduino.h>
#include <WiFi.h>

/* Layout: 640x172, 6px screen margin, 8px gaps (see docs/design/settings-layout.html).
 * Chinese labels use the built-in simsun_16_cjk font; Latin/numbers montserrat. */

enum SettingsView : uint8_t { SV_CLOSED, SV_MAIN, SV_EDIT };
enum EditField : uint8_t { EF_PASSWORD, EF_HOST, EF_PORT, EF_TOKEN };
enum ConnState : uint8_t { CS_IDLE, CS_CONNECTING, CS_OK, CS_FAIL };

static const uint32_t CONNECT_TIMEOUT_MS = 15000;
static const uint32_t OK_LINGER_MS = 600;
static const int MAX_NETS = 12;

static lv_obj_t *settingsScr;

static lv_obj_t *hdBack, *hdTitle, *hdStatDot, *hdStatSsid;
static lv_obj_t *wifiCap, *rescanBtn, *rescanLbl, *netList;
static lv_obj_t *rowHostV, *rowPortV, *rowTokenV, *saveBtn, *saveLbl;

static lv_obj_t *editPanel, *edClose, *edTitle, *edEye, *edGo, *edGoLbl;
static lv_obj_t *edTa, *edKb;

static SettingsView view = SV_CLOSED;
static EditField editField = EF_PASSWORD;
static ConnState connState = CS_IDLE;
static uint32_t connStart, okAt;
static bool scanning;
static String pendSsid, pendPass, oldSsid, oldPass;
static char ssidBuf[MAX_NETS][33];
static int8_t ssidRssi[MAX_NETS];
static int ssidCount;

static const lv_font_t *cjk() { return &lv_font_simsun_16_cjk; }

static void style_hit(lv_obj_t *o) {
  lv_obj_add_flag(o, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(o, 6, 0);
  lv_obj_set_style_border_width(o, 0, 0);
  lv_obj_set_style_shadow_width(o, 0, 0);
  lv_obj_set_style_pad_all(o, 0, 0);
}

static void style_pill(lv_obj_t *o) {
  style_hit(o);
  lv_obj_set_style_radius(o, 11, 0);
  lv_obj_set_style_pad_hor(o, 8, 0);
}

static void start_scan() {
  scanning = true;
  WiFi.scanNetworks(true);
  lv_obj_set_style_text_color(rescanLbl, UI_WARN, 0);
}

static void onRescan(lv_event_t *e) {
  LV_UNUSED(e);
  if (scanning) return;
  start_scan();
}

static void onBack(lv_event_t *e) {
  LV_UNUSED(e);
  ui_settings_close();
}

static void refresh_rows();

static void onSave(lv_event_t *e) {
  LV_UNUSED(e);
  ui_settings_close(); /* field edits persist on 完成; host/token apply live */
}

static void set_go_text(const char *txt, bool symbol) {
  lv_label_set_text(edGoLbl, txt);
  lv_obj_set_style_text_font(edGoLbl,
                             symbol ? &lv_font_montserrat_14 : cjk(), 0);
}

static void open_editor(EditField f) {
  editField = f;
  connState = CS_IDLE;
  view = SV_EDIT;

  lv_textarea_set_password_mode(edTa, f == EF_PASSWORD);
  lv_textarea_set_one_line(edTa, true);
  const char *title = "";
  switch (f) {
    case EF_PASSWORD:
      title = pendSsid.c_str();
      lv_textarea_set_text(edTa, "");
      break;
    case EF_HOST:
      title = "\xe4\xb8\xbb\xe6\x9c\xba"; /* 主机 */
      lv_textarea_set_text(edTa, config_host().c_str());
      break;
    case EF_PORT:
      title = "\xe7\xab\xaf\xe5\x8f\xa3"; /* 端口 */
      char pbuf[8];
      snprintf(pbuf, sizeof(pbuf), "%u", (unsigned)config_port());
      lv_textarea_set_text(edTa, pbuf);
      break;
    case EF_TOKEN:
      title = "\xe9\x9d\xa2\xe6\x9d\xbf\xe4\xbb\xa4\xe7\x89\x8c"; /* 面板令牌 */
      lv_textarea_set_text(edTa, config_token().c_str());
      break;
  }
  lv_label_set_text(edTitle, title);
  set_go_text(f == EF_PASSWORD ? "\xe8\xbf\x9e\xe6\x8e\xa5"      /* 连接 */
                               : "\xe5\xae\x8c\xe6\x88\x90",     /* 完成 */
              false);
  lv_obj_clear_flag(editPanel, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_style_border_color(edTa, ui_palette().track, 0);
}

static void close_editor() {
  lv_obj_add_flag(editPanel, LV_OBJ_FLAG_HIDDEN);
  view = SV_MAIN;
  refresh_rows();
}

static void apply_field() {
  String v = lv_textarea_get_text(edTa);
  v.trim();
  switch (editField) {
    case EF_HOST:
      if (v.length()) config_set_host(v);
      break;
    case EF_PORT: {
      long p = v.toInt();
      if (p > 0 && p <= 65535) config_set_port((uint16_t)p);
      break;
    }
    case EF_TOKEN:
      config_set_token(v);
      break;
    case EF_PASSWORD:
      break;
  }
}

static void try_connect() {
  pendPass = lv_textarea_get_text(edTa);
  oldSsid = config_wifi_ssid();
  oldPass = config_wifi_pass();
  connState = CS_CONNECTING;
  connStart = millis();
  set_go_text("...", true);
  WiFi.disconnect();
  WiFi.begin(pendSsid.c_str(), pendPass.c_str());
  Serial.println("wifi connect try " + pendSsid);
}

static void connect_result(bool ok) {
  if (ok) {
    connState = CS_OK;
    okAt = millis();
    set_go_text(LV_SYMBOL_OK, true);
    lv_obj_set_style_bg_color(edGo, UI_OK, 0);
  } else {
    connState = CS_FAIL;
    lv_obj_set_style_border_color(edTa, UI_BAD, 0);
    set_go_text(editField == EF_PASSWORD ? "\xe8\xbf\x9e\xe6\x8e\xa5" /* 连接 */
                                         : "\xe5\xae\x8c\xe6\x88\x90",/* 完成 */
                false);
    if (oldSsid.length()) {
      WiFi.disconnect();
      WiFi.begin(oldSsid.c_str(), oldPass.c_str());
    }
    Serial.println("wifi connect fail");
  }
}

static void onGo(lv_event_t *e) {
  LV_UNUSED(e);
  if (connState == CS_CONNECTING || connState == CS_OK) return;
  lv_obj_set_style_bg_color(edGo, UI_OK, 0);
  if (editField == EF_PASSWORD) {
    try_connect();
  } else {
    apply_field();
    close_editor();
  }
}

static void onEye(lv_event_t *e) {
  LV_UNUSED(e);
  lv_textarea_set_password_mode(edTa, !lv_textarea_get_password_mode(edTa));
}

static void onEdClose(lv_event_t *e) {
  LV_UNUSED(e);
  if (connState == CS_CONNECTING) return; /* let it finish or time out */
  close_editor();
}

static void onNet(lv_event_t *e) {
  if (connState == CS_CONNECTING) return;
  int idx = (int)(intptr_t)lv_event_get_user_data(e);
  if (idx < 0 || idx >= ssidCount) return;
  pendSsid = ssidBuf[idx];
  open_editor(EF_PASSWORD);
}

static void onRowHost(lv_event_t *e) { LV_UNUSED(e); open_editor(EF_HOST); }
static void onRowPort(lv_event_t *e) { LV_UNUSED(e); open_editor(EF_PORT); }
static void onRowToken(lv_event_t *e) { LV_UNUSED(e); open_editor(EF_TOKEN); }

/* ---------- construction ---------- */

static lv_obj_t *mk_pill(lv_obj_t *parent, int w, lv_event_cb_t cb) {
  lv_obj_t *p = lv_btn_create(parent);
  style_pill(p);
  if (w > 0) lv_obj_set_width(p, w);
  lv_obj_set_height(p, 22);
  if (cb) lv_obj_add_event_cb(p, cb, LV_EVENT_CLICKED, nullptr);
  return p;
}

static lv_obj_t *mk_cjk_lbl(lv_obj_t *parent, const char *txt) {
  lv_obj_t *l = ui_mk_lbl(parent, cjk(), lv_color_white());
  lv_label_set_text(l, txt);
  return l;
}

static lv_obj_t *mk_setting_row(lv_obj_t *parent, const char *key,
                                lv_event_cb_t cb) {
  lv_obj_t *row = lv_btn_create(parent);
  style_hit(row);
  lv_obj_set_height(row, 26);
  lv_obj_set_width(row, lv_pct(100));
  lv_obj_add_event_cb(row, cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t *k = mk_cjk_lbl(row, key);
  lv_obj_align(k, LV_ALIGN_LEFT_MID, 4, 0);
  lv_obj_t *chev = ui_mk_lbl(row, &lv_font_montserrat_12, lv_color_white());
  lv_label_set_text(chev, ">");
  lv_obj_align(chev, LV_ALIGN_RIGHT_MID, -4, 0);
  lv_obj_t *v = ui_mk_lbl(row, &lv_font_montserrat_12, lv_color_white());
  lv_obj_set_width(v, 150);
  lv_label_set_long_mode(v, LV_LABEL_LONG_DOT);
  lv_obj_align(v, LV_ALIGN_RIGHT_MID, -18, 0);
  return v;
}

static void build_header(lv_obj_t *scr) {
  lv_obj_t *hd = lv_obj_create(scr);
  ui_no_scroll(hd);
  lv_obj_set_size(hd, lv_pct(100), 24);
  lv_obj_set_style_bg_opa(hd, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(hd, 0, 0);
  lv_obj_set_style_pad_all(hd, 0, 0);

  hdBack = mk_pill(hd, 56, onBack);
  lv_obj_align(hdBack, LV_ALIGN_LEFT_MID, 2, 0);
  lv_obj_t *backLbl = mk_cjk_lbl(hdBack, "\xe8\xbf\x94\xe5\x9b\x9e"); /* 返回 */
  lv_obj_center(backLbl);

  hdTitle = mk_cjk_lbl(hd, "\xe8\xae\xbe\xe7\xbd\xae"); /* 设置 */
  lv_obj_align(hdTitle, LV_ALIGN_LEFT_MID, 66, 0);

  hdStatDot = lv_obj_create(hd);
  ui_no_scroll(hdStatDot);
  lv_obj_set_size(hdStatDot, 6, 6);
  lv_obj_set_style_radius(hdStatDot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(hdStatDot, UI_OK, 0);
  lv_obj_set_style_border_width(hdStatDot, 0, 0);
  lv_obj_align(hdStatDot, LV_ALIGN_RIGHT_MID, -100, 0);

  hdStatSsid = ui_mk_lbl(hd, &lv_font_montserrat_12, lv_color_white());
  lv_obj_set_width(hdStatSsid, 96);
  lv_label_set_long_mode(hdStatSsid, LV_LABEL_LONG_DOT);
  lv_obj_align(hdStatSsid, LV_ALIGN_RIGHT_MID, 0, 0);
}

static void build_main(lv_obj_t *scr) {
  lv_obj_t *cols = lv_obj_create(scr);
  ui_no_scroll(cols);
  lv_obj_set_size(cols, lv_pct(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(cols, LV_FLEX_FLOW_ROW);
  lv_obj_align(cols, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_style_bg_opa(cols, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(cols, 0, 0);
  lv_obj_set_style_pad_all(cols, 0, 0);
  lv_obj_set_style_pad_column(cols, 8, 0);

  /* WiFi card */
  lv_obj_t *wifiCard = lv_obj_create(cols);
  ui_style_card(wifiCard);
  lv_obj_set_size(wifiCard, 368, 136);
  lv_obj_set_flex_flow(wifiCard, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(wifiCard, 6, 0);
  lv_obj_set_style_pad_row(wifiCard, 3, 0);

  lv_obj_t *capRow = lv_obj_create(wifiCard);
  ui_no_scroll(capRow);
  lv_obj_set_size(capRow, lv_pct(100), 22);
  lv_obj_set_style_bg_opa(capRow, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(capRow, 0, 0);
  lv_obj_set_style_pad_all(capRow, 0, 0);
  wifiCap = mk_cjk_lbl(capRow, "WiFi \xe7\xbd\x91\xe7\xbb\x9c"); /* WiFi 网络 */
  lv_obj_align(wifiCap, LV_ALIGN_LEFT_MID, 2, 0);
  rescanBtn = mk_pill(capRow, 86, onRescan);
  lv_obj_align(rescanBtn, LV_ALIGN_RIGHT_MID, 0, 0);
  rescanLbl = mk_cjk_lbl(rescanBtn, "\xe9\x87\x8d\xe6\x96\xb0\xe6\x89\xab\xe6\x8f\x8f"); /* 重新扫描 */
  lv_obj_center(rescanLbl);

  netList = lv_obj_create(wifiCard);
  ui_no_scroll(netList);
  lv_obj_set_width(netList, lv_pct(100));
  lv_obj_set_flex_grow(netList, 1);
  lv_obj_set_flex_flow(netList, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(netList, 0, 0);
  lv_obj_set_style_pad_row(netList, 2, 0);
  lv_obj_set_style_bg_opa(netList, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(netList, 0, 0);

  /* Server card */
  lv_obj_t *srvCard = lv_obj_create(cols);
  ui_style_card(srvCard);
  lv_obj_set_size(srvCard, 258, 136);
  lv_obj_set_flex_flow(srvCard, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(srvCard, 8, 0);
  lv_obj_set_style_pad_row(srvCard, 2, 0);
  lv_obj_set_flex_align(srvCard, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);

  rowHostV = mk_setting_row(srvCard, "\xe4\xb8\xbb\xe6\x9c\xba", onRowHost);       /* 主机 */
  rowPortV = mk_setting_row(srvCard, "\xe7\xab\xaf\xe5\x8f\xa3", onRowPort);       /* 端口 */
  rowTokenV = mk_setting_row(srvCard, "\xe4\xbb\xa4\xe7\x89\x8c", onRowToken);     /* 令牌 */

  saveBtn = lv_btn_create(srvCard);
  style_hit(saveBtn);
  lv_obj_set_size(saveBtn, lv_pct(100), 26);
  lv_obj_add_event_cb(saveBtn, onSave, LV_EVENT_CLICKED, nullptr);
  saveLbl = mk_cjk_lbl(saveBtn, "\xe4\xbf\x9d\xe5\xad\x98\xe5\xb9\xb6\xe8\xbf\x94\xe5\x9b\x9e"); /* 保存并返回 */
  lv_obj_center(saveLbl);
}

static void build_editor(lv_obj_t *scr) {
  editPanel = lv_obj_create(scr);
  ui_no_scroll(editPanel);
  lv_obj_set_size(editPanel, lv_pct(100), lv_pct(100));
  lv_obj_set_flex_flow(editPanel, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(editPanel, 0, 0);
  lv_obj_set_style_pad_row(editPanel, 3, 0);
  lv_obj_set_style_bg_opa(editPanel, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(editPanel, 0, 0);
  lv_obj_add_flag(editPanel, LV_OBJ_FLAG_HIDDEN);

  lv_obj_t *top = lv_obj_create(editPanel);
  ui_no_scroll(top);
  lv_obj_set_size(top, lv_pct(100), 22);
  lv_obj_set_style_bg_opa(top, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(top, 0, 0);
  lv_obj_set_style_pad_all(top, 0, 0);
  lv_obj_set_style_pad_hor(top, 2, 0);

  edClose = lv_btn_create(top);
  style_hit(edClose);
  lv_obj_set_size(edClose, 24, 20);
  lv_obj_align(edClose, LV_ALIGN_LEFT_MID, 0, 0);
  lv_obj_add_event_cb(edClose, onEdClose, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *x = ui_mk_lbl(edClose, &lv_font_montserrat_14, lv_color_white());
  lv_label_set_text(x, LV_SYMBOL_CLOSE);
  lv_obj_center(x);

  edTitle = ui_mk_lbl(top, &lv_font_montserrat_12, lv_color_white());
  lv_obj_set_width(edTitle, 300);
  lv_label_set_long_mode(edTitle, LV_LABEL_LONG_DOT);
  lv_obj_align(edTitle, LV_ALIGN_LEFT_MID, 32, 0);

  edEye = lv_btn_create(top);
  style_hit(edEye);
  lv_obj_set_size(edEye, 26, 20);
  lv_obj_align(edEye, LV_ALIGN_RIGHT_MID, -62, 0);
  lv_obj_add_event_cb(edEye, onEye, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *eye = ui_mk_lbl(edEye, &lv_font_montserrat_14, lv_color_white());
  lv_label_set_text(eye, LV_SYMBOL_EYE_OPEN);
  lv_obj_center(eye);

  edGo = lv_btn_create(top);
  style_hit(edGo);
  lv_obj_set_size(edGo, 58, 20);
  lv_obj_align(edGo, LV_ALIGN_RIGHT_MID, 0, 0);
  lv_obj_add_event_cb(edGo, onGo, LV_EVENT_CLICKED, nullptr);
  edGoLbl = mk_cjk_lbl(edGo, "\xe8\xbf\x9e\xe6\x8e\xa5"); /* 连接 */
  lv_obj_center(edGoLbl);

  edTa = lv_textarea_create(editPanel);
  lv_obj_set_size(edTa, lv_pct(100), 26);
  lv_obj_set_style_pad_hor(edTa, 8, 0);
  lv_obj_set_style_border_width(edTa, 1, 0);
  lv_obj_set_style_radius(edTa, 6, 0);
  lv_textarea_set_one_line(edTa, true);
  lv_textarea_set_max_length(edTa, 63);
  lv_textarea_set_placeholder_text(edTa, "");

  edKb = lv_keyboard_create(editPanel);
  lv_obj_set_width(edKb, lv_pct(100));
  lv_obj_set_flex_grow(edKb, 1);
  lv_keyboard_set_textarea(edKb, edTa);
  lv_keyboard_set_popovers(edKb, false);
  lv_obj_set_style_bg_opa(edKb, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(edKb, 0, 0);
  lv_obj_set_style_pad_all(edKb, 2, 0);
  lv_obj_set_style_pad_gap(edKb, 3, 0);
  lv_obj_set_style_radius(edKb, 0, 0);
}

/* ---------- list rendering ---------- */

static int bars_for(int32_t rssi) {
  if (rssi >= -55) return 4;
  if (rssi >= -67) return 3;
  if (rssi >= -80) return 2;
  return 1;
}

static void add_placeholder_row(const char *txt) {
  lv_obj_t *ph = lv_obj_create(netList);
  ui_no_scroll(ph);
  lv_obj_set_size(ph, lv_pct(100), 24);
  lv_obj_set_style_radius(ph, 6, 0);
  lv_obj_set_style_border_width(ph, 0, 0);
  lv_obj_t *l = ui_mk_lbl(ph, &lv_font_montserrat_12, lv_color_white());
  lv_label_set_text(l, txt);
  lv_obj_center(l);
}

static void refresh_rows() {
  lv_obj_clean(netList);
  if (scanning) {
    add_placeholder_row(". . .");
    return;
  }
  if (ssidCount == 0) {
    add_placeholder_row("no networks");
    return;
  }
  const String cur = WiFi.SSID();
  for (int i = 0; i < ssidCount; i++) {
    lv_obj_t *row = lv_btn_create(netList);
    style_hit(row);
    lv_obj_set_size(row, lv_pct(100), 24);
    lv_obj_add_event_cb(row, onNet, LV_EVENT_CLICKED, (void *)(intptr_t)i);

    lv_obj_t *s = ui_mk_lbl(row, &lv_font_montserrat_12, lv_color_white());
    lv_label_set_text(s, ssidBuf[i]);
    lv_obj_set_width(s, 220);
    lv_label_set_long_mode(s, LV_LABEL_LONG_DOT);
    lv_obj_align(s, LV_ALIGN_LEFT_MID, 4, 0);

    int bars = bars_for(ssidRssi[i]);
    for (int b = 0; b < 4; b++) {
      lv_obj_t *bar = lv_obj_create(row);
      ui_no_scroll(bar);
      lv_obj_set_size(bar, 3, 3 + b * 2);
      lv_obj_align(bar, LV_ALIGN_RIGHT_MID, -10 - (3 - b) * 5, 0);
      lv_obj_set_style_radius(bar, 1, 0);
      lv_obj_set_style_border_width(bar, 0, 0);
      lv_obj_set_style_bg_color(bar, ui_palette().ink2, 0);
      lv_obj_set_style_bg_opa(bar, b < bars ? LV_OPA_COVER : LV_OPA_20, 0);
    }
    if (cur == ssidBuf[i]) {
      lv_obj_t *dot = lv_obj_create(row);
      ui_no_scroll(dot);
      lv_obj_set_size(dot, 6, 6);
      lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
      lv_obj_set_style_bg_color(dot, UI_OK, 0);
      lv_obj_set_style_border_width(dot, 0, 0);
      lv_obj_align(dot, LV_ALIGN_RIGHT_MID, -38, 0);
      lv_obj_t *ok = ui_mk_lbl(row, &lv_font_montserrat_12, lv_color_white());
      lv_label_set_text(ok, LV_SYMBOL_OK);
      lv_obj_set_style_text_color(ok, UI_OK, 0);
      lv_obj_align(ok, LV_ALIGN_RIGHT_MID, -50, 0);
    }
  }
}

static void take_scan() {
  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING) return;
  scanning = false;
  lv_obj_set_style_text_color(rescanLbl, ui_palette().ink, 0);
  ssidCount = 0;
  if (n > 0) {
    /* strongest-first, deduped by SSID */
    int order[n];
    for (int i = 0; i < n; i++) order[i] = i;
    for (int i = 0; i < n - 1; i++) {
      for (int j = i + 1; j < n; j++) {
        if (WiFi.RSSI(order[j]) > WiFi.RSSI(order[i])) {
          int t = order[i];
          order[i] = order[j];
          order[j] = t;
        }
      }
    }
    for (int k = 0; k < n && ssidCount < MAX_NETS; k++) {
      String s = WiFi.SSID(order[k]);
      if (!s.length()) continue;
      bool dup = false;
      for (int d = 0; d < ssidCount; d++) {
        if (s == ssidBuf[d]) {
          dup = true;
          break;
        }
      }
      if (dup) continue;
      strlcpy(ssidBuf[ssidCount], s.c_str(), sizeof(ssidBuf[0]));
      ssidRssi[ssidCount] = (int8_t)WiFi.RSSI(order[k]);
      ssidCount++;
    }
  }
  WiFi.scanDelete();
  refresh_rows();
}

/* ---------- public API ---------- */

void ui_settings_open() {
  if (view != SV_CLOSED) return;
  if (!settingsScr) {
    settingsScr = lv_obj_create(nullptr);
    lv_obj_set_style_pad_all(settingsScr, 6, 0);
    ui_no_scroll(settingsScr);
    build_header(settingsScr);
    build_main(settingsScr);
    build_editor(settingsScr);
  }
  view = SV_MAIN;
  connState = CS_IDLE;
  ui_settings_refresh_theme();
  lv_screen_load(settingsScr);
  ui_pet_set_paused(true);
  start_scan();
  refresh_rows();
}

void ui_settings_close() {
  if (view == SV_CLOSED) return;
  view = SV_CLOSED;
  connState = CS_IDLE;
  lv_obj_add_flag(editPanel, LV_OBJ_FLAG_HIDDEN);
  lv_screen_load(uiScr);
  ui_pet_set_paused(false);
}

bool ui_settings_active() { return view != SV_CLOSED; }

void ui_settings_poll() {
  if (view == SV_CLOSED) return;

  if (scanning) take_scan();

  if (view == SV_EDIT && connState == CS_CONNECTING) {
    if (WiFi.status() == WL_CONNECTED && WiFi.SSID() == pendSsid) {
      config_set_wifi(pendSsid, pendPass);
      net_reconnect(); /* reset mDNS/ntp state for the new network */
      connect_result(true);
    } else if (millis() - connStart > CONNECT_TIMEOUT_MS) {
      connect_result(false);
    }
    return;
  }
  if (view == SV_EDIT && connState == CS_OK && millis() - okAt > OK_LINGER_MS) {
    close_editor();
    return;
  }

  if (view == SV_MAIN) {
    static uint32_t lastHd;
    if (millis() - lastHd > 1000) {
      lastHd = millis();
      bool up = WiFi.status() == WL_CONNECTED;
      lv_obj_set_style_bg_color(hdStatDot, up ? UI_OK : UI_BAD, 0);
      lv_label_set_text(hdStatSsid,
                        up ? WiFi.SSID().c_str() : config_wifi_ssid().c_str());
    }
  }
}

void ui_settings_refresh_theme() {
  if (!settingsScr) return;
  const UiPalette &p = ui_palette();

  lv_obj_set_style_bg_color(settingsScr, p.bg, 0);
  lv_obj_set_style_bg_color(hdBack, p.track, 0);
  lv_obj_set_style_text_color(hdTitle, p.ink, 0);
  lv_obj_set_style_text_color(hdStatSsid, p.ink2, 0);

  /* pills */
  lv_obj_set_style_bg_color(rescanBtn, p.track, 0);
  lv_obj_set_style_text_color(rescanLbl, scanning ? UI_WARN : p.ink, 0);
  lv_obj_set_style_text_color(wifiCap, p.ink, 0);

  /* server rows: style the row bg for hover-less flat look */
  lv_obj_t *rows[] = {lv_obj_get_parent(rowHostV), lv_obj_get_parent(rowPortV),
                      lv_obj_get_parent(rowTokenV)};
  for (lv_obj_t *r : rows) {
    lv_obj_set_style_bg_color(r, p.bg, 0);
    lv_obj_t *k = lv_obj_get_child(r, 0);   /* key label (cjk) */
    lv_obj_t *ch = lv_obj_get_child(r, 1);  /* chevron */
    lv_obj_t *v = lv_obj_get_child(r, 2);   /* value label */
    lv_obj_set_style_text_color(k, p.ink2, 0);
    lv_obj_set_style_text_color(v, p.ink, 0);
    lv_obj_set_style_text_color(ch, p.ink2, 0);
  }

  lv_obj_set_style_bg_color(saveBtn, UI_OK, 0);
  lv_obj_set_style_text_color(saveLbl, lv_color_white(), 0);

  /* editor */
  lv_obj_set_style_bg_color(editPanel, p.bg, 0);
  lv_obj_set_style_bg_color(edClose, p.track, 0);
  lv_obj_set_style_bg_color(edEye, p.track, 0);
  lv_obj_set_style_text_color(edTitle, p.ink, 0);
  lv_obj_set_style_bg_color(edGo, UI_OK, 0);
  lv_obj_set_style_text_color(edGoLbl, lv_color_white(), 0);
  lv_obj_set_style_text_color(edTa, p.ink, 0);
  lv_obj_set_style_bg_color(edTa, p.bg, 0);
  lv_obj_set_style_border_color(edTa, p.track, 0);
  lv_obj_set_style_text_color(lv_textarea_get_label(edTa), p.ink, 0);

  /* keyboard keys via parts */
  lv_obj_set_style_bg_color(edKb, p.bg, 0);
  lv_obj_set_style_bg_color(edKb, p.card, LV_PART_ITEMS);
  lv_obj_set_style_border_color(edKb, p.track, LV_PART_ITEMS);
  lv_obj_set_style_border_width(edKb, 1, LV_PART_ITEMS);
  lv_obj_set_style_text_color(edKb, p.ink, LV_PART_ITEMS);
  lv_obj_set_style_bg_color(edKb, p.track, LV_PART_ITEMS | LV_STATE_PRESSED);

  refresh_rows();
}
