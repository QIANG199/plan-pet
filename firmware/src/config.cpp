#include "config.h"
#include "net.h"
#include "standby.h"
#include "ui/ui.h"
#include "ui/ui_settings.h"
#include "power.h"
#include "bsp/lcd_bl_pwm_bsp.h"
#include "bsp/lvgl_port.h"
#include <Preferences.h>
#include "secrets.h"

static Preferences prefs;
static String ssid, pass, token, host;
static uint16_t port = SECRET_PORT;
static bool dark = true;
static uint8_t bright = 255;
static uint8_t sleepT = 5; /* standby minutes after the pet falls asleep */
static String lineBuf;

static void applyBright() {
  lcd_bl_set_brightness(bright);
}

static void load() {
  ssid = prefs.getString("ssid", SECRET_WIFI_SSID);
  pass = prefs.getString("pass", SECRET_WIFI_PASS);
  token = prefs.getString("token", SECRET_PANEL_TOKEN);
  host = prefs.getString("host", SECRET_HOST);
  port = prefs.getUShort("port", SECRET_PORT);
  dark = prefs.getBool("dark", true);
  bright = (uint8_t)prefs.getUChar("bright", 255);
  if (bright < 8) bright = 8;
  sleepT = (uint8_t)prefs.getUChar("sleepT", 5);
  if (sleepT > 90) sleepT = 90;
}

static void save() {
  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);
  prefs.putString("token", token);
  prefs.putString("host", host);
  prefs.putUShort("port", port);
}

static void printCfg() {
  Serial.println("ssid=" + ssid);
  Serial.println("host=" + host + ":" + String(port));
  Serial.println("token=" + String(token.length() ? "(set)" : "(empty)"));
  Serial.printf("bright=%u\n", (unsigned)bright);
  Serial.printf("sleep=%umin\n", (unsigned)sleepT);
}

static void handleLine(String line) {
  line.trim();
  if (line.length() == 0) return;
  standby_request_wake(); /* any serial command lights the screen back up */
  int sp = line.indexOf(' ');
  String cmd = sp < 0 ? line : line.substring(0, sp);
  String arg = sp < 0 ? "" : line.substring(sp + 1);
  cmd.toUpperCase();

  if (cmd == "WIFI") {
    ssid = arg;
    save();
    Serial.println("ok ssid; connecting");
    net_reconnect();
  } else if (cmd == "PASS") {
    pass = arg;
    save();
    Serial.println("ok pass; connecting");
    net_reconnect();
  } else if (cmd == "TOKEN") {
    token = arg;
    save();
    Serial.println("ok token");
  } else if (cmd == "HOST") {
    host = arg;
    save();
    Serial.println("ok host");
  } else if (cmd == "PORT") {
    long p = arg.toInt();
    if (p > 0 && p <= 65535) {
      config_set_port((uint16_t)p);
      Serial.println("ok port");
    } else {
      Serial.println("bad port; 1-65535");
    }
  } else if (cmd == "PET") {
    arg.toLowerCase();
    ui_set_pet_override(arg.c_str());
    Serial.println(arg.length() ? "ok pet " + arg : "ok pet auto");
  } else if (cmd == "BRIGHT") {
    if (arg.length()) {
      int v = arg.toInt();
      if (v < 8) v = 8;
      if (v > 255) v = 255;
      config_set_bright((uint8_t)v);
    }
    Serial.printf("ok bright %u\n", (unsigned)bright);
  } else if (cmd == "SLEEP") {
    if (arg.equalsIgnoreCase("NOW")) {
      standby_request_sleep_now();
      Serial.println("ok sleep now");
    } else if (arg.length()) {
      int v = arg.toInt();
      if (v < 0) v = 0;
      if (v > 90) v = 90;
      config_set_sleep_timeout((uint8_t)v);
      Serial.printf("ok sleep %umin\n", (unsigned)sleepT);
    } else {
      Serial.printf("sleep %umin (0=off, NOW=blank now)\n", (unsigned)sleepT);
    }
  } else if (cmd == "OFF") {
    power_request_shutdown();
    Serial.println("ok off countdown; PWR press cancels");
  } else if (cmd == "REBOOT") {
    Serial.println("ok reboot");
    delay(50);
    ESP.restart();
  } else if (cmd == "SETUP") {
    /* Dev shortcut: open the settings page over serial; "SETUP EDIT <1-3>"
       opens a field editor, "SETUP CLOSE" leaves the page. */
    if (lvgl_port_lock(200)) {
      ui_settings_open();
      if (arg.length()) {
        int sp2 = arg.indexOf(' ');
        String sub = sp2 < 0 ? arg : arg.substring(0, sp2);
        String val = sp2 < 0 ? "" : arg.substring(sp2 + 1);
        if (sub == "EDIT") ui_settings_diag_editor(val.toInt());
        else if (sub == "CLOSE") ui_settings_close();
      }
      lvgl_port_unlock();
    }
    Serial.println("ok setup");
  } else if (cmd == "SHOW") {
    printCfg();
  } else if (cmd == "HELP") {
    Serial.println("WIFI <ssid>");
    Serial.println("PASS <password>");
    Serial.println("TOKEN <panel-token>");
    Serial.println("HOST <pc-ip>");
    Serial.println("PORT <1-65535>");
    Serial.println("PET <idle|thinking|typing|happy|error|sleeping|auto>");
    Serial.println("BRIGHT <8-255>");
    Serial.println("SLEEP <0-90>|NOW");
    Serial.println("OFF");
    Serial.println("REBOOT");
    Serial.println("SHOW");
  } else {
    Serial.println("unknown; HELP");
  }
}

void config_begin() {
  prefs.begin("panel", false);
  load();
  applyBright();
  Serial.println("serial: WIFI / PASS / TOKEN / HOST / PET / BRIGHT / SLEEP / SHOW");
  printCfg();
}

void config_poll_serial() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      handleLine(lineBuf);
      lineBuf = "";
    } else if (lineBuf.length() < 160) {
      lineBuf += c;
    }
  }
}

String config_wifi_ssid() { return ssid; }
String config_wifi_pass() { return pass; }
String config_token() { return token; }
String config_host() { return host; }
uint16_t config_port() { return port; }
bool config_dark() { return dark; }

void config_set_dark(bool next) {
  dark = next;
  prefs.putBool("dark", dark);
}

void config_set_wifi(const String &newSsid, const String &newPass) {
  ssid = newSsid;
  pass = newPass;
  save();
}

void config_set_host(const String &newHost) {
  host = newHost;
  save();
}

void config_set_token(const String &newToken) {
  token = newToken;
  save();
}

void config_set_port(uint16_t newPort) {
  port = newPort;
  save();
}

uint8_t config_bright() { return bright; }

void config_set_bright(uint8_t v) {
  if (v < 8) v = 8;
  bright = v;
  prefs.putUChar("bright", bright);
  applyBright();
}

uint8_t config_sleep_timeout() { return sleepT; }

void config_set_sleep_timeout(uint8_t v) {
  if (v > 90) v = 90;
  sleepT = v;
  prefs.putUChar("sleepT", sleepT);
}
