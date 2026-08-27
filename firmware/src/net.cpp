#include "net.h"
#include "config.h"
#include "ui/ui.h"
#include "bsp/lvgl_port.h"
#include "rtc.h"
#include <Network.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

static Snapshot snap;
static uint32_t lastPoll;
static uint32_t pollMs = 1000;
static uint32_t lastOkMs;
static uint32_t lastNtpTry;
static uint32_t lastWifiTry;
static uint32_t wifiBackoff = 2000;
static bool ntpOk;
static int lastHttp = -999;

static const uint32_t SRV_LOST_MS = 15000; /* miss this many polls in a row -> server lost */

/* Screen-off power saving: WiFi is normally off and only comes up in short
 * windows (one good poll per window) so the radio is not parked on the AP
 * beacon stream all night. */
static const uint32_t SLEEP_NET_PERIOD_MS = 5UL * 60UL * 1000UL;
static const uint32_t SLEEP_NET_WINDOW_MS = 15000;
static const uint32_t POLL_FAIL_MAX_BACKOFF = 60; /* x pollMs, caps retry storms while the server is down */

static bool sleepNet;
static bool radioWindow;
static bool windowPolled;
static uint32_t windowStart;
static uint32_t nextWindowAt;
static uint32_t failBackoff = 1;

static void parseDash(const String &body) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body);
  snap.fresh = false;
  if (err) {
    Serial.printf("json %s\n", err.c_str());
    return;
  }

  snap.srvTs = (time_t)(doc["ts"] | 0);
  snap.petSince = (time_t)(doc["pet"]["since"] | 0);

  JsonObject glm = doc["glm"];
  snap.glmOk = glm["ok"] | false;
  snap.glmPeak = snap.glmOk && (glm["peak"] | false);
  snap.h5.present = snap.glmOk && !glm["h5"].isNull();
  if (snap.h5.present) {
    snap.h5.pct = glm["h5"]["pct"] | 0;
    snap.h5.resetAt = (time_t)(glm["h5"]["nextResetAt"] | 0);
  }
  snap.week.present = snap.glmOk && !glm["week"].isNull();
  if (snap.week.present) {
    snap.week.pct = glm["week"]["pct"] | 0;
    snap.week.resetAt = (time_t)(glm["week"]["nextResetAt"] | 0);
  }

  JsonObject cur = doc["cursor"];
  snap.cursorOk = cur["ok"] | false;
  snap.autoBar.present = snap.cursorOk && !cur["auto"].isNull();
  if (snap.autoBar.present) snap.autoBar.pct = cur["auto"]["pct"] | 0;
  snap.apiBar.present = snap.cursorOk && !cur["api"].isNull();
  if (snap.apiBar.present) snap.apiBar.pct = cur["api"]["pct"] | 0;
  snap.cycleEnd = (time_t)(cur["cycleEnd"] | 0);

  snap.petAgent = doc["pet"]["agent"] | "";
  snap.petState = doc["pet"]["state"] | "idle";
  snap.glmDot = doc["pet"]["dots"]["zcode"] | false;
  snap.curDot = doc["pet"]["dots"]["cursor"] | false;
  snap.fresh = true;
  lastOkMs = millis();
}

static bool usableV4(const IPAddress &ip) {
  if (ip[0] == 0 || ip[0] == 127) return false;
  if (ip[0] == 169 && ip[1] == 254) return false;
  return true;
}

static bool resolveHost(IPAddress &ip) {
  String host = config_host();
  host.trim();
  if (ip.fromString(host)) return true;
  if (host.length() && WiFi.hostByName(host.c_str(), ip) == 1 && usableV4(ip)) {
    return true;
  }
  return false;
}

static int pollOnce() {
  if (WiFi.status() != WL_CONNECTED) return -1;
  if (config_token().isEmpty()) return -1;

  IPAddress ip;
  if (!resolveHost(ip)) {
    Serial.println("dns fail " + config_host() + " (try HOST <pc-ip>)");
    lastHttp = -2;
    return -2;
  }

  String url = "http://" + ip.toString() + ":" + String(config_port()) + "/api/dashboard";
  HTTPClient http;
  http.setTimeout(4000);
  http.begin(url);
  http.addHeader("X-Panel-Token", config_token());
  int code = http.GET();
  if (code == 200) {
    parseDash(http.getString());
  }
  http.end();
  if (code != lastHttp || code != 200) {
    Serial.printf("http %d %s glm=%d\n", code, ip.toString().c_str(), (int)snap.glmOk);
    lastHttp = code;
  }
  return code;
}

void net_reconnect() {
  String s = config_wifi_ssid();
  if (!s.length()) return;
  WiFi.disconnect();
  WiFi.setAutoReconnect(true);
  WiFi.begin(s.c_str(), config_wifi_pass().c_str());
  wifiBackoff = 2000;
  lastWifiTry = millis();
  Serial.println("wifi connecting " + s);
}

void net_begin() {
  snap.petState = "idle";
  lastOkMs = millis(); /* grace period: no "server lost" right after boot */
  WiFi.mode(WIFI_STA);
  net_reconnect();
  if (!config_wifi_ssid().length()) {
    Serial.println("wifi unset; send WIFI <ssid> then PASS <password>");
  }
}

void net_sleep_enter() {
  sleepNet = true;
  radioWindow = false;
  windowPolled = false;
  WiFi.mode(WIFI_OFF);
  nextWindowAt = millis() + SLEEP_NET_PERIOD_MS;
  Serial.println("[net] screen-sleep duty cycle");
}

void net_sleep_exit() {
  if (!sleepNet) return;
  sleepNet = false;
  radioWindow = false;
  WiFi.mode(WIFI_STA);
  net_reconnect();
}

void net_loop() {
  uint32_t now = millis();
  if (sleepNet) {
    if (!radioWindow) {
      if ((int32_t)(now - nextWindowAt) >= 0) {
        radioWindow = true;
        windowStart = now;
        windowPolled = false;
        WiFi.mode(WIFI_STA);
        net_reconnect();
        lastPoll = now - pollMs; /* poll as soon as the link comes up */
      } else {
        rtc_save_if_synced();
        return;
      }
    } else if (windowPolled || now - windowStart > SLEEP_NET_WINDOW_MS) {
      radioWindow = false;
      WiFi.mode(WIFI_OFF);
      nextWindowAt = now + SLEEP_NET_PERIOD_MS;
      return;
    }
  }
  if (WiFi.status() != WL_CONNECTED && config_wifi_ssid().length()) {
    if (now - lastWifiTry >= wifiBackoff) {
      lastWifiTry = now;
      WiFi.reconnect();
      Serial.printf("wifi retry backoff=%lu\n", (unsigned long)wifiBackoff);
      if (wifiBackoff < 60000) wifiBackoff *= 2;
    }
  } else if (WiFi.status() == WL_CONNECTED) {
    wifiBackoff = 2000;
  }
  if (WiFi.status() == WL_CONNECTED && !ntpOk && now - lastNtpTry > 10000) {
    lastNtpTry = now;
    configTzTime("CST-8", "ntp.aliyun.com", "pool.ntp.org");
    ntpOk = true;
  }
  rtc_save_if_synced();
  if (now - lastPoll < pollMs * failBackoff) return;
  lastPoll = now;
  int code = pollOnce();
  if (code == 200) {
    failBackoff = 1;
    if (radioWindow) windowPolled = true; /* one good poll is all a window needs */
  } else if (failBackoff < POLL_FAIL_MAX_BACKOFF) {
    failBackoff = failBackoff > 1 ? failBackoff * 2 : 2;
  }
  if (lvgl_port_lock(50)) {
    ui_apply(snap);
    ui_set_link(net_wifi_up(), net_server_ok());
    lvgl_port_unlock();
  }
}

bool net_wifi_up() { return WiFi.status() == WL_CONNECTED; }
const Snapshot &net_snapshot() { return snap; }
uint32_t net_last_ok_ms() { return lastOkMs; }
bool net_server_ok() {
  return WiFi.status() == WL_CONNECTED && millis() - lastOkMs < SRV_LOST_MS;
}
void net_set_poll_ms(uint32_t ms) { pollMs = ms < 1000 ? 1000 : ms; }
