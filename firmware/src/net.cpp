#include "net.h"
#include "config.h"
#include "ui.h"
#include "lvgl_port.h"
#include "rtc.h"
#include <Network.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

static Snapshot snap;
static uint32_t lastPoll;
static uint32_t lastNtpTry;
static uint32_t lastWifiTry;
static uint32_t wifiBackoff = 2000;
static bool ntpOk;
static bool mdnsReady;
static int lastHttp = -999;

static void parseDash(const String &body) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body);
  snap.fresh = false;
  if (err) {
    Serial.printf("json %s\n", err.c_str());
    return;
  }

  JsonObject glm = doc["glm"];
  snap.glmOk = glm["ok"] | false;
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
}

static bool usableV4(const IPAddress &ip) {
  if (ip[0] == 0 || ip[0] == 127) return false;
  if (ip[0] == 169 && ip[1] == 254) return false;
  return true;
}

static String mdnsLabel(const String &host) {
  String q = host;
  q.trim();
  q.toLowerCase();
  if (q.endsWith(".local")) q.remove(q.length() - 6);
  return q;
}

static void ensureMdns() {
  if (mdnsReady || WiFi.status() != WL_CONNECTED) return;
  mdnsReady = MDNS.begin("quota-panel");
  Serial.println(mdnsReady ? "mdns on" : "mdns begin fail");
}

static bool resolveHost(IPAddress &ip) {
  String host = config_host();
  host.trim();
  if (ip.fromString(host)) return true;

  ensureMdns();
  const String label = mdnsLabel(host);
  if (mdnsReady && label.length()) {
    const int n = MDNS.queryService("http", "tcp");
    for (int i = 0; i < n; i++) {
      const String inst = MDNS.instanceName(i);
      const String hn = MDNS.hostname(i);
      if (!inst.equalsIgnoreCase("desktop-pet") && !hn.startsWith(label)) continue;
      IPAddress found = MDNS.address(i);
      if (usableV4(found)) {
        ip = found;
        return true;
      }
    }
    IPAddress found = MDNS.queryHost(label.c_str(), 2000);
    if (usableV4(found)) {
      ip = found;
      return true;
    }
  }

  if (WiFi.hostByName(host.c_str(), ip) == 1 && usableV4(ip)) return true;
  return false;
}

static void pollOnce() {
  if (WiFi.status() != WL_CONNECTED) return;
  if (config_token().isEmpty()) return;

  IPAddress ip;
  if (!resolveHost(ip)) {
    Serial.println("dns fail " + config_host() + " (try HOST <pc-ip>)");
    lastHttp = -2;
    return;
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
}

void net_reconnect() {
  if (mdnsReady) {
    MDNS.end();
    mdnsReady = false;
  }
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
  WiFi.mode(WIFI_STA);
  net_reconnect();
  if (!config_wifi_ssid().length()) {
    Serial.println("wifi unset; send WIFI <ssid> then PASS <password>");
  }
}

void net_loop() {
  uint32_t now = millis();
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
  if (now - lastPoll < 2000) return;
  lastPoll = now;
  pollOnce();
  if (lvgl_port_lock(50)) {
    ui_apply(snap);
    ui_set_wifi(WiFi.status() == WL_CONNECTED);
    lvgl_port_unlock();
  }
}

bool net_wifi_up() { return WiFi.status() == WL_CONNECTED; }
const Snapshot &net_snapshot() { return snap; }
