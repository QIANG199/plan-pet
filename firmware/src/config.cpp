#include "config.h"
#include "net.h"
#include "ui.h"
#include <Preferences.h>
#include "secrets.h"

static Preferences prefs;
static String ssid, pass, token, host;
static uint16_t port = SECRET_PORT;
static bool dark = true;
static String lineBuf;

static void load() {
  ssid = prefs.getString("ssid", SECRET_WIFI_SSID);
  pass = prefs.getString("pass", SECRET_WIFI_PASS);
  token = prefs.getString("token", SECRET_PANEL_TOKEN);
  host = prefs.getString("host", SECRET_HOST);
  port = prefs.getUShort("port", SECRET_PORT);
  dark = prefs.getBool("dark", true);
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
}

static void handleLine(String line) {
  line.trim();
  if (line.length() == 0) return;
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
  } else if (cmd == "PET") {
    arg.toLowerCase();
    ui_set_pet_override(arg.c_str());
    Serial.println(arg.length() ? "ok pet " + arg : "ok pet auto");
  } else if (cmd == "SHOW") {
    printCfg();
  } else if (cmd == "HELP") {
    Serial.println("WIFI <ssid>");
    Serial.println("PASS <password>");
    Serial.println("TOKEN <panel-token>");
    Serial.println("HOST <desktop-pet.local or pc-ip>");
    Serial.println("PET <idle|thinking|typing|happy|error|sleeping|auto>");
    Serial.println("SHOW");
  } else {
    Serial.println("unknown; HELP");
  }
}

void config_begin() {
  prefs.begin("panel", false);
  load();
  Serial.println("serial: WIFI / PASS / TOKEN / HOST / PET / SHOW");
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
